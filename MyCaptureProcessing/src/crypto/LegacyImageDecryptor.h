// =============================================================================
// 文件: LegacyImageDecryptor.h
// 作用: 声明旧下位机图像字节的逆 S-Box 解密器。
// =============================================================================
#pragma once

#include <cstddef>

namespace meyer
{
    namespace captureprocessing
    {
        class LegacyImageDecryptor
        {
        public:
            // 从 headerBytes 开始原地替换所有字节，前面数据头保持不变。
            static bool DecryptPlane(unsigned char* image,
                                     std::size_t imageBytes,
                                     std::size_t headerBytes);

        private:
            // 纯静态工具类不应创建对象。
            LegacyImageDecryptor();
        };
    }
}
