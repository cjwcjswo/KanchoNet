#include "NetworkEngine.h"
#include "../Platform.h"

// 플랫폼별 네트워크 모델 include
#ifdef KANCHONET_PLATFORM_WINDOWS
    #include "../Network/IOCPModel.h"
    #include "../Network/RIOModel.h"
#elif defined(KANCHONET_PLATFORM_LINUX)
    #include "../Network/EpollModel.h"
    #include "../Network/IOUringModel.h"
#endif

// 템플릿 명시적 인스턴스화
// 컴파일러가 각 플랫폼의 네트워크 모델을 사용하는 NetworkEngine을 생성하도록 함

namespace KanchoNet
{
    #ifdef KANCHONET_PLATFORM_WINDOWS
        // Windows: IOCPModel과 RIOModel 인스턴스화
        template class NetworkEngine<IOCPModel>;
        template class NetworkEngine<RIOModel>;
    #elif defined(KANCHONET_PLATFORM_LINUX)
        // Linux: EpollModel과 IOUringModel 인스턴스화
        template class NetworkEngine<EpollModel>;
        template class NetworkEngine<IOUringModel>;
    #endif

} // namespace KanchoNet

