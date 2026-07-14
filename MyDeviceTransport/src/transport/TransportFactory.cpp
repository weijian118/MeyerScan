// 把传输枚举映射到具体实现；当前只注册 CyAPI USB。
#include "../stdafx.h"

#include "TransportFactory.h"

#include "ITransport.h"
#include "cyapi/CyApiTransport.h"

namespace meyer
{
    namespace device
    {
        // 查询是否存在可实例化实现，不把规划中的传输方式伪装为可用。
        bool TransportFactory::Supports(TransportType type)
        {
            switch (type)
            {
            case TransportType::CyApiUsb:
                return true;

            case TransportType::Unknown:
            default:
                return false;
            }
        }

        // 使用 unique_ptr 把传输对象所有权明确交给 DeviceSession。
        std::unique_ptr<ITransport> TransportFactory::Create(TransportType type)
        {
            switch (type)
            {
            case TransportType::CyApiUsb:
                return std::unique_ptr<ITransport>(new CyApiTransport());

            case TransportType::Unknown:
            default:
                return std::unique_ptr<ITransport>();
            }
        }
    }
}
