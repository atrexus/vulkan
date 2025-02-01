#include "pe/image_fixer.hpp"
#include "spdlog/spdlog.h"

uint32_t ImageUtils::getChecksum(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", path);
        return 0;
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(file_size);
    file.read(buffer.data(), file_size);
    file.close();

    DWORD sum = 0;
    WORD* data = reinterpret_cast<WORD*>(buffer.data());
    size_t num_words = file_size / sizeof(WORD);

    for (size_t i = 0; i < num_words; ++i) {
        sum += data[i];
        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    if (file_size % sizeof(WORD)) {
        sum += (static_cast<WORD>(buffer[file_size - 1]) << 8);
        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }

    return ~sum;
}

bool ImageUtils::fixImage(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", path);
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(file_size);
    file.read(buffer.data(), file_size);
    file.close();

    IMAGE_DOS_HEADER* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer.data());
    IMAGE_NT_HEADERS* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(buffer.data() + dos_header->e_lfanew);

    if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        spdlog::error("Bad PE file: invalid NT signature");
        return false;
    }

    spdlog::info("NT Headers signature: {:x}", nt_headers->Signature);

    nt_headers->OptionalHeader.CheckSum = 0;

    uint32_t section_alignment = nt_headers->OptionalHeader.SectionAlignment;
    IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt_headers);

    for (size_t i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i) {
        sections[i].VirtualAddress = (sections[i].VirtualAddress / section_alignment) * section_alignment;
        sections[i].SizeOfRawData = (sections[i].SizeOfRawData / 512) * 512;
        spdlog::info("Section {} - VirtualAddress: {:x}, SizeOfRawData: {:x}", i + 1, sections[i].VirtualAddress, sections[i].SizeOfRawData);
    }

    spdlog::info("First section's Virtual Address: {:x}", sections->VirtualAddress);

    DWORD checksum = ImageUtils::getChecksum(path.c_str());
    nt_headers->OptionalHeader.CheckSum = checksum;

    spdlog::info("Calculated checksum: {:x}", nt_headers->OptionalHeader.CheckSum);

    std::ofstream output_file(path, std::ios::binary);
    if (!output_file.is_open()) {
        spdlog::error("Failed to open output file: {}", path);
        return false;
    }

    output_file.write(buffer.data(), buffer.size());
    output_file.close();

    spdlog::info("Saved the fixed image to: {}", path);
    return true;
}
