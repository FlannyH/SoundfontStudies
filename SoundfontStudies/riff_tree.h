#pragma once
#include <string>
#include "structs.h"

namespace Flan {
    struct RiffNode {
        ChunkId id;
        u32 size = 0;
        u8* data = nullptr;
        std::vector<RiffNode> subchunks;
        RiffNode& operator[](const std::string& name);
        RiffNode& operator[](size_t index);
        bool exists(const std::string& name);
        void visualize_tree(std::vector<bool>& draw_line, int depth = 0);
        void get_subchunks(ChunkDataHandler& data_handler);
    };

    class RiffTree {
    public:
        RiffNode riff_chunk;
        u8* data = nullptr;
        bool from_file(const std::string& path);
        void visualize_tree();
        RiffNode& operator[](const std::string& c);
    };
}