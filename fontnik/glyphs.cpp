// fontnik
#include "glyphs.hpp"
#include <protozero/pbf_reader.hpp>
#include <protozero/pbf_writer.hpp>
// node
#include <limits>
#include <memory>
#include <string>
#include<iostream>

// sdf-glyph-foundry
#include <gzip/compress.hpp>
#include <gzip/decompress.hpp>
#include <gzip/utils.hpp>
#include <mapbox/glyph_foundry.hpp>
#include <mapbox/glyph_foundry_impl.hpp>
#include <utility>

namespace node_fontnik {



struct GlyphPBF {
    explicit GlyphPBF(char  const* buffer, std::size_t length)
        : data{buffer, length}
    {
        
    }

    // non-copyable
    GlyphPBF(GlyphPBF const&) = delete;
    GlyphPBF& operator=(GlyphPBF const&) = delete;

    // non-movable
    GlyphPBF(GlyphPBF&&) = delete;
    GlyphPBF& operator=(GlyphPBF&&) = delete;

    protozero::data_view data;
};

struct ft_library_guard {
    // non copyable
    ft_library_guard(ft_library_guard const&) = delete;
    ft_library_guard& operator=(ft_library_guard const&) = delete;

    explicit ft_library_guard(FT_Library* lib) : library_(lib) {}

    ~ft_library_guard() {
        if (library_ != nullptr) {
            FT_Done_FreeType(*library_);
        }
    }

    FT_Library* library_;
};

struct ft_face_guard {
    // non copyable
    ft_face_guard(ft_face_guard const&) = delete;
    ft_face_guard& operator=(ft_face_guard const&) = delete;
    explicit ft_face_guard(FT_Face* f) : face_(f) {}

    ~ft_face_guard() {
        if (face_ != nullptr) {
            FT_Done_Face(*face_);
        }
    }

    FT_Face* face_;
};

struct AsyncLoad{
    AsyncLoad(const std::string& font_file)
        :font_file_(font_file){
        
    }

    void Execute(){
        try {
            FT_Library library = nullptr;
            ft_library_guard library_guard(&library);
            FT_Error error = FT_Init_FreeType(&library);
            if (error != 0) {
                //LCOV_EXCL_START
                printf("could not open FreeType library");
                return;
                // LCOV_EXCL_END
            }
            FT_Face ft_face = nullptr;
            FT_Long num_faces = 0;
            for (int i = 0; ft_face == nullptr || i < num_faces; ++i) {
                ft_face_guard face_guard(&ft_face);
                FT_Error face_error = FT_New_Face(library, font_file_.c_str(), i, &ft_face);
                if (face_error != 0) {
                    std::cerr << "could not open font file"<< '\n';
                    return;
                }
                if (num_faces == 0) {
                    num_faces = ft_face->num_faces;
                    faces_.reserve(static_cast<std::size_t>(num_faces));
                }
                if (ft_face->family_name != nullptr) {
                    std::set<int> points;
                    FT_ULong charcode;
                    FT_UInt gindex;
                    charcode = FT_Get_First_Char(ft_face, &gindex);
                    while (gindex != 0) {
                        charcode = FT_Get_Next_Char(ft_face, charcode, &gindex);
                        if (charcode != 0) points.emplace(charcode);
                    }
                    std::vector<int> points_vec(points.begin(), points.end());
                    if (ft_face->style_name != nullptr) {
                        faces_.emplace_back(ft_face->family_name, ft_face->style_name, std::move(points_vec));
                    } else {
                        faces_.emplace_back(ft_face->family_name, std::move(points_vec));
                    }
                } else {
                    std::cerr << "font does not have family_name or style_name"<< '\n';
                    return;
                }
            }
        } catch (std::exception const& ex) {
            std::cerr << ex.what() << '\n';
        }
    }

    
   const std::vector<FaceMetadata>& GetResult()const {
       
        return faces_;
    }
  

  private:
    std::string font_file_;
    std::vector<FaceMetadata> faces_;
};

struct AsyncRange{
   
    
    AsyncRange(const std::string& font_file, std::uint32_t start, std::uint32_t end)
        : font_file_(font_file),
          start_(start), end_(end){
              
          }

    void Execute(){
        try {
            unsigned array_size = end_ - start_;
            chars_.reserve(array_size);
            for (unsigned i = start_; i <= end_; ++i) {
                chars_.emplace_back(i);
            }

            FT_Library library = nullptr;
            ft_library_guard library_guard(&library);
            FT_Error error = FT_Init_FreeType(&library);
            if (error != 0) {
                // LCOV_EXCL_START
                std::cerr << "could not open FreeType library"<< std::endl;
                return;
                // LCOV_EXCL_END
            }

            protozero::pbf_writer pbf_writer{message_};
            FT_Face ft_face = nullptr;
            FT_Long num_faces = 0;
            for (int i = 0; ft_face == nullptr || i < num_faces; ++i) {
                ft_face_guard face_guard(&ft_face);
                FT_Error face_error = FT_New_Face(library, font_file_.c_str(), i, &ft_face);
                if (face_error != 0) {
                    std::cerr <<"could not open font"<< std::endl;
                    return;
                }

                if (num_faces == 0) num_faces = ft_face->num_faces;

                if (ft_face->family_name != nullptr) {
                    protozero::pbf_writer fontstack_writer{pbf_writer, 1};
                    if (ft_face->style_name != nullptr) {
                        fontstack_writer.add_string(1, std::string(ft_face->family_name) + " " + std::string(ft_face->style_name));
                    } else {
                        fontstack_writer.add_string(1, std::string(ft_face->family_name));
                    }
                    fontstack_writer.add_string(2, std::to_string(start_) + "-" + std::to_string(end_));

                    const double scale_factor = 1.0;

                    // Set character sizes.
                    double size = 24 * scale_factor;
                    FT_Set_Char_Size(ft_face, 0, static_cast<FT_F26Dot6>(size * (1 << 6)), 0, 0);

                    for (std::vector<uint32_t>::size_type x = 0; x != chars_.size(); ++x) {
                        FT_ULong char_code = chars_[x];
                        sdf_glyph_foundry::glyph_info glyph;
                        // Get FreeType face from face_ptr.
                        FT_UInt char_index = FT_Get_Char_Index(ft_face, char_code);
                        if (char_index == 0U) continue;

                        glyph.glyph_index = char_index;
                        sdf_glyph_foundry::RenderSDF(glyph, 24, 3, 0.25, ft_face);

                        // Add glyph to fontstack.
                        protozero::pbf_writer glyph_writer{fontstack_writer, 3};

                        // shortening conversion
                        if (char_code > std::numeric_limits<FT_ULong>::max()) {
                            std::cerr <<"Invalid value for char_code: too large"<< std::endl;
                            return;
                        }
                        glyph_writer.add_uint32(1, static_cast<std::uint32_t>(char_code));

                        if (glyph.width > 0) glyph_writer.add_bytes(2, glyph.bitmap);

                        // direct type conversions, no need for checking or casting
                        glyph_writer.add_uint32(3, glyph.width);
                        glyph_writer.add_uint32(4, glyph.height);
                        glyph_writer.add_sint32(5, glyph.left);

                        // conversions requiring checks, for safety and correctness

                        // double to int
                        double top = static_cast<double>(glyph.top) - glyph.ascender;
                        if (top < std::numeric_limits<std::int32_t>::min() || top > std::numeric_limits<std::int32_t>::max()) {
                            std::cerr <<"Invalid value for glyph.top-glyph.ascender"<< std::endl;
                            return;
                        }
                        glyph_writer.add_sint32(6, static_cast<std::int32_t>(top));

                        // double to uint
                        if (glyph.advance < std::numeric_limits<std::uint32_t>::min() || glyph.advance > std::numeric_limits<std::uint32_t>::max()) {
                            std::cerr <<"Invalid value for glyph.top-glyph.ascender"<< std::endl;
                            return;
                        }
                        glyph_writer.add_uint32(7, static_cast<std::uint32_t>(glyph.advance));
                    }
                } else {
                    std::cerr <<"font does not have family_name"<< std::endl;
                    return;
                }
            }
        } catch (std::exception const& ex) {
            std::cerr << ex.what()<< std::endl;
        }
    }

    
    const std::string& GetResult() const{
        return   message_;
    }
    

  private:
    const std::string font_file_;
    std::uint32_t     start_;
    std::uint32_t     end_;
    
    std::vector<std::uint32_t> chars_;
    
    std::string message_;
};

const std::vector<FaceMetadata>& Load(const std::string& font_file) {
   
    auto* worker = new AsyncLoad(font_file);
    worker->Execute();
    
   return worker->GetResult();
    
}

const std::string&  Range(const std::string& font_file, std::uint32_t start, std::uint32_t end) {
    
    auto* worker = new AsyncRange(font_file, start, end);
    worker->Execute();
    
    return   worker->GetResult();
}


struct AsyncComposite{
    

    AsyncComposite(std::vector<std::unique_ptr<GlyphPBF>>&& glyphs)
        :glyphs_(std::move(glyphs)),
          message_(std::make_unique<std::string>()) {}

    void Execute(){
        try {
            std::vector<std::unique_ptr<std::vector<char>>> buffer_cache;
            std::map<std::uint32_t, protozero::data_view> id_mapping;
            bool first_buffer = true;
            std::string fontstack_name;
            std::string range;
            protozero::pbf_writer pbf_writer(*message_);
            protozero::pbf_writer fontstack_writer{pbf_writer, 1};
            // TODO(danespringmeyer): avoid duplicate fontstacks to be sent it
            for (auto const& glyph_obj : glyphs_) {
                protozero::data_view data_view{};
                if (gzip::is_compressed(glyph_obj->data.data(), glyph_obj->data.size())) {
                    buffer_cache.push_back(std::make_unique<std::vector<char>>());
                    gzip::Decompressor decompressor;
                    decompressor.decompress(*buffer_cache.back(), glyph_obj->data.data(), glyph_obj->data.size());
                    data_view = protozero::data_view{buffer_cache.back()->data(), buffer_cache.back()->size()};
                } else {
                    data_view = glyph_obj->data;
                }
                protozero::pbf_reader fontstack_reader(data_view);
                while (fontstack_reader.next(1)) {
                    auto stack_reader = fontstack_reader.get_message();
                    while (stack_reader.next()) {
                        switch (stack_reader.tag()) {
                        case 1: // name
                        {
                            if (first_buffer) {
                                fontstack_name = stack_reader.get_string();
                            } else {
                                fontstack_name = fontstack_name + ", " + stack_reader.get_string();
                            }
                            break;
                        }
                        case 2: // range
                        {
                            if (first_buffer) {
                                range = stack_reader.get_string();
                            } else {
                                stack_reader.skip();
                            }
                            break;
                        }
                        case 3: // glyphs
                        {
                            auto glyphs_data = stack_reader.get_view();
                            // collect all ids from first
                            if (first_buffer) {
                                protozero::pbf_reader glyphs_reader(glyphs_data);
                                std::uint32_t glyph_id;
                                while (glyphs_reader.next(1)) {
                                    glyph_id = glyphs_reader.get_uint32();
                                }
                                id_mapping.emplace(glyph_id, glyphs_data);
                            } else {
                                protozero::pbf_reader glyphs_reader(glyphs_data);
                                std::uint32_t glyph_id;
                                while (glyphs_reader.next(1)) {
                                    glyph_id = glyphs_reader.get_uint32();
                                }
                                auto search = id_mapping.find(glyph_id);
                                if (search == id_mapping.end()) {
                                    id_mapping.emplace(glyph_id, glyphs_data);
                                }
                            }
                            break;
                        }
                        default:
                            // ignore data for unknown tags to allow for future extensions
                            stack_reader.skip();
                        }
                    }
                }
                first_buffer = false;
            }
            fontstack_writer.add_string(1, fontstack_name);
            fontstack_writer.add_string(2, range);
            for (auto const& glyph_pair : id_mapping) {
                fontstack_writer.add_message(3, glyph_pair.second);
            }
        } catch (std::exception const& ex) {
            std::cerr << ex.what()<< std::endl;
        }
    }

    std::unique_ptr<std::string> GetResult() {
      
        return std::move(message_);
    }


  private:
    std::vector<std::unique_ptr<GlyphPBF>> glyphs_;
    std::unique_ptr<std::string> message_;
};

std::unique_ptr<std::string> Composite(std::vector<std::unique_ptr<GlyphPBF>>&& glyphs) {
  
    auto* worker = new AsyncComposite(std::move(glyphs));
    worker->Execute();
    return worker->GetResult();
}

} // namespace node_fontnik
