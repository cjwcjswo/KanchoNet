#include "EngineConfig.h"

namespace KanchoNet
{
    bool EngineConfig::Validate() const
    {
        // 포트 범위 확인 (1024 이상 권장)
        if (mPort == 0 || mPort < 1024)
        {
            return false;
        }

        // 최대 세션 수 확인
        if (mMaxSessions == 0 || mMaxSessions > 100000)
        {
            return false;
        }

        // Backlog 확인
        if (mBacklog == 0 || mBacklog > 10000)
        {
            return false;
        }

        // 버퍼 크기 확인
        if (mSendBufferSize < 1024 || mSendBufferSize > 10 * 1024 * 1024) // 1KB ~ 10MB
        {
            return false;
        }

        if (mRecvBufferSize < 1024 || mRecvBufferSize > 10 * 1024 * 1024) // 1KB ~ 10MB
        {
            return false;
        }

        // RIO 설정 확인
        if (mRioReceiveBufferCount == 0 || mRioReceiveBufferCount > 100000)
        {
            return false;
        }

        if (mRioSendBufferCount == 0 || mRioSendBufferCount > 100000)
        {
            return false;
        }

        if (mRioCQSize < 128 || mRioCQSize > 1000000)
        {
            return false;
        }

        return true;
    }

} // namespace KanchoNet

