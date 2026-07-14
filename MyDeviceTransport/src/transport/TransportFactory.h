#pragma once

#include <memory>

#include "../api/DeviceTypes.h"

namespace meyer
{
    namespace device
    {
        class ITransport;

        class TransportFactory
        {
        public:
            // 判断当前版本是否实现指定传输类型。
            static bool Supports(TransportType type);
            // 创建具体传输对象；不支持时返回空 unique_ptr。
            static std::unique_ptr<ITransport> Create(TransportType type);
        };
    }
}
