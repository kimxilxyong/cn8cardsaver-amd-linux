/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      Lee Clagett <https://github.com/vtnerd>
 * Copyright 2016-2018 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <thread>

 // This cl_ext is provided as part of the AMD APP SDK
//#include <CL/cl.h>
#include <CL/cl_ext.h>

#include "amd/OclCache.h"
#include "amd/OclError.h"
#include "amd/OclLib.h"
#include "amd/OclGPU.h"
#include "amd/AdlUtils.h"
#include "common/Platform.h"
#include "crypto/CryptoNight.h"
#include "workers/Handle.h"
#include "workers/OclThread.h"
#include "workers/OclWorker.h"
#include "workers/Workers.h"
#include "common/log/Log.h"
#include "3rdparty/ADL/adl_sdk.h"
#include "3rdparty/ADL/adl_defines.h"
#include "3rdparty/ADL/adl_structures.h"



OclWorker::OclWorker(Handle *handle) :
    m_id(handle->threadId()),
    m_threads(handle->totalWays()),
    m_ctx(handle->ctx()),
    m_hashCount(0),
    m_timestamp(0),
    m_count(0),
    m_sequence(0),
    m_blob()
{
    const int64_t affinity = handle->config()->affinity();
    m_thread = static_cast<OclThread *>(handle->config());

    if (affinity >= 0) {
        Platform::setThreadAffinity(affinity);
    }
}



void OclWorker::start()
{
    char offset[2000];
    int iReduceMining = 0;
    int iSleepFactor = 1000;
    int LastTemp = 0;
    int temp;
    int NeedCooling = 0;
    bool IsCoolingEnabled = false;;
    cl_uint results[0x100];
    //ADL_CONTEXT_HANDLE context;
    CoolingContext cool;

    m_thread->setThreadId(m_id);

    cool.pciBus = m_ctx->device_pciBusID;
    cool.Card = m_ctx->deviceIdx;

    Workers::addWorkercount();

    IsCoolingEnabled = AdlUtils::InitADL(&cool);
    if (IsCoolingEnabled == false) {
        LOG_WARN("Cooling is disabled for thread %i!", id());
    }
    else {
        IsCoolingEnabled = AdlUtils::Get_DeviceID_by_PCI(&cool, m_thread);
        if (!IsCoolingEnabled) {
            LOG_ERR("Failed get_deviceid_by_pci DeviceId %i pciDeviceID %02x pciBusID %02x pciDomainID %02x", cool.Card, m_thread->pciDeviceID(), m_thread->pciBusID(), m_thread->pciDomainID());
        }
        AdlUtils::GetMaxFanRpm(&cool);
        m_thread->setCardId(cool.Card);

    }
    while (Workers::sequence() > 0) {
        if (Workers::isPaused()) {
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            } while (Workers::isPaused());

            if (Workers::sequence() == 0) {
                break;
            }

            consumeJob();
        }
        int LastTemp;
        if (IsCoolingEnabled)
            AdlUtils::DoCooling(m_ctx->DeviceID, m_ctx->deviceIdx, m_id, &cool);

        while (!Workers::isOutdated(m_sequence)) {

            //LOG_INFO("**Card %u Temperature %i iReduceMining %i iSleepFactor %i LastTemp %i NeedCooling %i ", m_ctx->deviceIdx, temp, iReduceMining, iSleepFactor, LastTemp, NeedCooling);
            if (IsCoolingEnabled)
                AdlUtils::DoCooling(m_ctx->DeviceID, m_ctx->deviceIdx, m_id, &cool);

            memset(results, 0, sizeof(cl_uint) * (0x100));

            XMRRunJob(m_ctx, results, m_job.algorithm().variant());

            for (size_t i = 0; i < results[0xFF]; i++) {
                *m_job.nonce() = results[i];
                m_job.setTemp(cool.CurrentTemp);
                m_job.setFan(cool.CurrentFan);
                m_job.setNeedscooling(cool.NeedsCooling);
                m_job.setSleepfactor(cool.SleepFactor);
                m_job.setCard(cool.Card);
                m_job.setThreadId(m_id);
                Workers::submit(m_job);
            }

            m_count += m_ctx->rawIntensity;

            storeStats();
            std::this_thread::yield();
        }

        consumeJob();
    }
    if (IsCoolingEnabled)
        AdlUtils::ReleaseADL(&cool);

    Workers::removeWorkercount();
}


bool OclWorker::resume(const Job &job)
{
    if (m_job.poolId() == -1 && job.poolId() >= 0 && job.id() == m_pausedJob.id()) {
        m_job = m_pausedJob;
        m_nonce = m_pausedNonce;
        return true;
    }

    return false;
}


void OclWorker::consumeJob()
{
    Job job = Workers::job();
    m_sequence = Workers::sequence();
    if (m_job == job) {
        return;
    }

    save(job);

    if (resume(job)) {
        setJob();
        return;
    }

    m_job = std::move(job);
    m_job.setThreadId(m_id);

    if (m_job.isNicehash()) {
        m_nonce = (*m_job.nonce() & 0xff000000U) + (0xffffffU / m_threads * m_id);
    }
    else {
        m_nonce = 0xffffffffU / m_threads * m_id;
    }

    m_ctx->Nonce = m_nonce;

    setJob();
}


void OclWorker::save(const Job &job)
{
    if (job.poolId() == -1 && m_job.poolId() >= 0) {
        m_pausedJob = m_job;
        m_pausedNonce = m_nonce;
    }
}


void OclWorker::setJob()
{
    memcpy(m_blob, m_job.blob(), sizeof(m_blob));

    XMRSetJob(m_ctx, m_blob, m_job.size(), m_job.target(), m_job.algorithm().variant());
}


void OclWorker::storeStats()
{
    using namespace std::chrono;

    const uint64_t timestamp = time_point_cast<milliseconds>(high_resolution_clock::now()).time_since_epoch().count();
    m_hashCount.store(m_count, std::memory_order_relaxed);
    m_timestamp.store(timestamp, std::memory_order_relaxed);
}
