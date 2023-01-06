#include "riff_tree.h"
#include <fstream>

namespace Flan {
    RiffNode& RiffNode::operator[](const std::string& name) {
        // Search through the subchunks to find any with a matching name, and if it doesn't exist, throw an exception
        for (RiffNode& node : subchunks) {
            if (node.id == name.c_str()) {
                return node;
            }
        }
        throw std::invalid_argument("Attempted to access a subnode that does not exist!");
    }

    RiffNode& RiffNode::operator[](const size_t index) {
        // Search through the subchunks to find any with a matching name, and if it doesn't exist, throw an exception
        if (index >= subchunks.size())
            throw std::invalid_argument("Attempted to access a subnode that does not exist!");
        return subchunks[index];
    }

    bool RiffNode::exists(const std::string& name) {
        // Search through the subchunks to find any with a matching name
        for (RiffNode& node : subchunks) {
            if (node.id == name.c_str()) {
                return true;
            }
        }
        return false;
    }

    void RiffNode::visualize_tree(std::vector<bool>& draw_line, const int depth) {
        // Set console code page to UTF-8 so console known how to interpret string data
        //SetConsoleOutputCP(CP_UTF8);

        // Enable buffering to prevent VS from chopping up UTF-8 byte sequences
        const int result = setvbuf(stdout, nullptr, _IOFBF, 1000);
        (void)result;

        // For each subchunk
        for (size_t i = 0; i < subchunks.size(); i++) {

            bool is_last = false;
            // Print indentation
            for (int x = 1; x < depth; x++) {
                if (draw_line[x+1])
                    printf("|      ");
                else
                    printf("       ");
            }
            // Print fancy tree characters
            if (depth > 0 && i < subchunks.size() - 1) {
                printf("|----- ");
            }
            else if (depth > 0) {
                printf("------ ");
                is_last = true;
            }
            draw_line.push_back(!is_last);
            // Print info about subchunk
            if (subchunks[i].subchunks.empty()) {
                printf("CHUNK:%s (%u bytes)\n", subchunks[i].id.c_str().get(), subchunks[i].size);
            }
            else {
                printf("LIST:%s (%u bytes)\n", subchunks[i].id.c_str().get(), subchunks[i].size);
                subchunks[i].visualize_tree(draw_line, depth + 1);
            }
            draw_line.pop_back();
        }

            
    }

    void RiffNode::get_subchunks(ChunkDataHandler& data_handler) {
        // Loop over all the subchunks and sublists
        Chunk chunk;
        while (chunk.from_chunk_data_handler(data_handler)) {
            // For lists
            if (chunk.id == "LIST") {
                // Get the list name
                ChunkId list_name;
                data_handler.get_data(&list_name, sizeof(ChunkId));

                // Create a chunk data handler for this list
                ChunkDataHandler subchunk_data;
                subchunk_data.from_data_handler(data_handler, chunk.size - 4);

                // Create a new list node, and recursively get its subchunks
                RiffNode new_node;
                new_node.id = list_name;
                new_node.size = chunk.size - 4;
                new_node.data = data_handler.data_pointer;
                new_node.get_subchunks(subchunk_data);

                // Advance the current nodes' data pointer
                data_handler.get_data(nullptr, chunk.size - 4);

                // Add that list node to the current node
                subchunks.push_back(new_node);
            }
            // For chunks
            else {
                // Add the chunk, including its data, to the subchunk node
                RiffNode new_node;
                new_node.id = chunk.id;
                new_node.size = chunk.size;
                new_node.data = data_handler.data_pointer;
                data_handler.get_data(nullptr, chunk.size);
                subchunks.push_back(new_node);
            }

            // Enforce 16-bit alignment requirement
            if (chunk.size % 2 == 1) {
                data_handler.get_data(nullptr, 1);
            }
        }
    }

    bool RiffTree::from_file(const std::string& path) {
        // Open file
        std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
        if (!input.good()) return false;

        // Load RIFF header
        RiffChunk riff_header;
        input.read(reinterpret_cast<char*>(&riff_header), sizeof(riff_header));

        // Verify if this is in fact a RIFF header
        if (riff_header.type != "RIFF") return false;

        // If all is good, read the entire file into a chunk handler
        ChunkDataHandler riff_data;
        const bool result = riff_data.from_ifstream(input, riff_header.size, false);
        if (!result) return false;
        data = riff_data.data_pointer;

        // Create a RIFF node
        riff_chunk.id = riff_header.name;
        riff_chunk.size = riff_header.size;

        // Get subchunks
        riff_chunk.get_subchunks(riff_data);
        return true;
    }

    void RiffTree::visualize_tree() {
        printf("RIFF\n");
        std::vector<bool> draw_line{false};
        draw_line.push_back(true);
        riff_chunk.visualize_tree(draw_line, 1);
    }

    RiffNode& RiffTree::operator[](const std::string& c) {
        return riff_chunk[c];
    }
}
