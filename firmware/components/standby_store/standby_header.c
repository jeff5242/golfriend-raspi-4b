#include "standby_header.h"

size_t standby_max_image_size(size_t partition_size)
{
    return partition_size > sizeof(standby_header_t) ? partition_size - sizeof(standby_header_t) : 0;
}

bool standby_header_valid(const standby_header_t *header, size_t partition_size)
{
    return header != NULL && header->magic == STANDBY_HEADER_MAGIC && header->size > 0 &&
           header->size <= standby_max_image_size(partition_size);
}

bool standby_header_matches(const standby_header_t *header, uint32_t size, uint32_t crc32)
{
    return header != NULL && header->magic == STANDBY_HEADER_MAGIC && header->size == size &&
           header->crc32 == crc32;
}
