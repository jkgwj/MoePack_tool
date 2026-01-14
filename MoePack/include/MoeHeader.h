#pragma once
#include<cstdint>
#include<cstring>
#include <stdexcept>


static const char MOE_PackVersion[8] = { 'v','0','.','1','.','0','0','\0' }; // v0.1.0(0)，最后一位标识位

// 字节序转换
namespace MOE_Endian {
    // 判断当前平台是否为小端
    inline bool is_host_little_endian() {
        union {
            uint32_t i;
            uint8_t  c[4];
        } test = { 0x01020304 };
        return test.c[0] == 0x04;
    }

    // 主机序 → 大端
    inline uint32_t htobe32(uint32_t host32) {
        if (is_host_little_endian()) {
            return ((host32 & 0x000000FF) << 24) |
                ((host32 & 0x0000FF00) << 8) |
                ((host32 & 0x00FF0000) >> 8) |
                ((host32 & 0xFF000000) >> 24);
        }
        return host32;
    }

    inline uint64_t htobe64(uint64_t host64) {
        if (is_host_little_endian()) {
            return ((host64 & 0x00000000000000FFULL) << 56) |
                ((host64 & 0x000000000000FF00ULL) << 40) |
                ((host64 & 0x0000000000FF0000ULL) << 24) |
                ((host64 & 0x00000000FF000000ULL) << 8) |
                ((host64 & 0x000000FF00000000ULL) >> 8) |
                ((host64 & 0x0000FF0000000000ULL) >> 24) |
                ((host64 & 0x00FF000000000000ULL) >> 40) |
                ((host64 & 0xFF00000000000000ULL) >> 56);
        }
        return host64;
    }

    // 大端 → 主机序
    inline uint32_t betoh32(uint32_t be32) {
        return htobe32(be32); // 小端反转两次=原数，大端直接返回
    }

    inline uint64_t betoh64(uint64_t be64) {
        return htobe64(be64);
    }
}

#pragma pack(push, 1) // 紧凑存储

struct MoeHeader//以大端模式存储
{
	char     magic[8]= { 'M','O','E','_','A','R','C','\0' };          // 文件 "MOE_ARC"
	char     version[8];                                              // 文件版本号
	uint32_t header_size= sizeof(MoeHeader);                          // 头部大小
	uint8_t  ztsd_on=1;                                               // 是否使用 ZTSD 压缩（0 = 否，1 = 是）
	uint8_t  encrypted_on=0;                                          // 是否加密（0 = 否，1 = 是）
	uint8_t check_data[64];                                           // 校验数据
	uint64_t data_size = 0;                                           // 数据大小（字节）

	
	MoeHeader() {
		memcpy(version, MOE_PackVersion, 8); // 手动拷贝版本号数组
	}
	MoeHeader(const MoeHeader& other) {
		memcpy(this, &other, sizeof(MoeHeader));
	}
	MoeHeader(const uint8_t* _check_data, size_t check_size, uint64_t data_size) {
		memcpy(version, MOE_PackVersion, 8); 
		ztsd_on = 1;
		encrypted_on = 0;
		set_check_data(_check_data, check_size);
		this->data_size = data_size;
	}
	//设置校验数据
	void set_check_data(const uint8_t* data, size_t size) {
        assert(size <= sizeof(check_data));
		if (size > sizeof(check_data)) {
			size = sizeof(check_data); // 限制大小不超过64字节
		}
		memcpy(check_data, data, size);
	}
	// 设置数据大小
	void set_data_size(uint64_t size) {
		data_size = size;
	}
    // 将多字节字段转为大端
    void to_big_endian() {
        header_size = MOE_Endian::htobe32(header_size);
        data_size = MOE_Endian::htobe64(data_size);
    }

    // 将多字节字段从大端转回主机序
    void from_big_endian() {
        header_size = MOE_Endian::betoh32(header_size);
        data_size = MOE_Endian::betoh64(data_size);
    }

    // 校验"MOE_ARC"是否合法
    bool is_magic_valid() const {
        return memcmp(magic, "MOE_ARC", 6) == 0;
    }
};
#pragma pack(pop)

static_assert(sizeof(MoeHeader) == 94, "MoeHeader 大小错误！请检查#pragma pack和字段定义");