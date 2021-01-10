#pragma once


#include <protozero/pbf_reader.hpp>
#include <protozero/pbf_writer.hpp>

#include <vector>
#include <memory>


namespace node_fontnik {

struct GlyphPBF;


struct FaceMetadata {
    
    // movaable only for highest efficiency
    FaceMetadata& operator=(FaceMetadata&& c) = default;
    FaceMetadata(FaceMetadata&& c) = default;
    
    std::string family_name{};
    std::string style_name{};
    std::vector<int> points{};
    
    FaceMetadata(std::string _family_name,
                 std::string _style_name,
                 std::vector<int>&& _points)
    : family_name(std::move(_family_name)),
    style_name(std::move(_style_name)),
    points(std::move(_points)) {
        
    }
    
    FaceMetadata(std::string _family_name,
                 std::vector<int>&& _points)
    : family_name(std::move(_family_name)),
    points(std::move(_points)) {
        
    }
    
private:// non copyable
    FaceMetadata(FaceMetadata const&) = delete;
    FaceMetadata& operator=(FaceMetadata const&) = delete;
    
}; 

const std::vector<FaceMetadata>& Load(const std::string& font_file);

const std::string& Range(const std::string& font_file, std::uint32_t start, std::uint32_t end);

std::unique_ptr<std::string> Composite(std::vector<std::unique_ptr<GlyphPBF>>&& glyphs);

} // namespace node_fontnik
