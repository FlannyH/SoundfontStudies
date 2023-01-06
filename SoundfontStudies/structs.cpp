#include "structs.h"
#include <fstream>

namespace Flan {

    std::shared_ptr<char> ChunkId::c_str() const {
        std::shared_ptr<char> cstr(static_cast<char*>(malloc(5)), free);
        cstr.get()[0] = id_chr[0];
        cstr.get()[1] = id_chr[1];
        cstr.get()[2] = id_chr[2];
        cstr.get()[3] = id_chr[3];
        cstr.get()[4] = 0;
        return cstr;
    }

    bool ChunkId::verify(const char* other_id) const {
        if (*this != other_id) {
            printf("[ERROR] Chunk ID mismatch! Expected '%s'\n", other_id);
        }
        return *this == other_id;
    }

    bool ChunkDataHandler::from_buffer(uint8_t* buffer_to_use, uint32_t size) {
        if (buffer_to_use == nullptr)
            return false;
        if (size == 0) {
            return false;
        }
        data_pointer = buffer_to_use;
        chunk_bytes_left = size;
        return true;
    }

    bool ChunkDataHandler::from_file(FILE* file, uint32_t size) {
        if (size == 0) {
            return false;
        }
        original_pointer = static_cast<uint8_t*>(malloc(size));
        data_pointer = original_pointer;
        if (!data_pointer) { return false; }
        fread_s(data_pointer, size, size, 1, file);
        chunk_bytes_left = size;
        return true;
    }

    bool ChunkDataHandler::from_ifstream(std::ifstream& file, uint32_t size, bool free_on_destruction) {
        if (size == 0) {
            return false;
        }
        data_pointer = static_cast<uint8_t*>(malloc(size));
        if (free_on_destruction)
            original_pointer = data_pointer;
        if (!data_pointer) { return false; }
        file.read(reinterpret_cast<char*>(data_pointer), size);
        chunk_bytes_left = size;
        return true;
    }

    bool ChunkDataHandler::from_data_handler(const ChunkDataHandler& data, const uint32_t size) {
        if (size == 0) {
            return false;
        }
        data_pointer = data.data_pointer;
        chunk_bytes_left = size;
        return true;
    }

    bool ChunkDataHandler::get_data(void* destination, const uint32_t byte_count) {
        if (chunk_bytes_left >= byte_count) {
            if (destination != nullptr) { // Sending a nullptr is valid, it just discards byte_count number of bytes
                memcpy(destination, data_pointer, byte_count);
            }
            chunk_bytes_left -= byte_count;
            data_pointer += byte_count;
            return true;
        }
        return false;
    }
}
