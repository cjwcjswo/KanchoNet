#include "EngineConfig.h"

namespace KanchoNet
{
    bool EngineConfig::Validate() const
    {
        // 포트 범위 확인 (1024 이상 권장)
        if (port == 0 || port < 1024)
        {
            return false;
        }

        // 최대 세션 수 확인
        if (maxSessions == 0 || maxSessions > 100000)
        {
            return false;
        }

        // Backlog 확인
        if (backlog == 0 || backlog > 10000)
        {
            return false;
        }

        // 버퍼 크기 확인
        if (sendBufferSize < 1024 || sendBufferSize > 10 * 1024 * 1024) // 1KB ~ 10MB
        {
            return false;
        }

        if (recvBufferSize < 1024 || recvBufferSize > 10 * 1024 * 1024) // 1KB ~ 10MB
        {
            return false;
        }

        // RIO 설정 확인
        if (rioReceiveBufferCount == 0 || rioReceiveBufferCount > 100000)
        {
            return false;
        }

        if (rioSendBufferCount == 0 || rioSendBufferCount > 100000)
        {
            return false;
        }

        if (rioCQSize < 128 || rioCQSize > 1000000)
        {
            return false;
        }

        return true;
    }

} // namespace KanchoNet

