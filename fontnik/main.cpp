

#include "glyphs.hpp"

 
#include <fstream>
#include <iostream>
#include <sstream>


//从文件读入到string里
std::string  readFile(const std::string& filename) {
    
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.good()) {
        return   std::string("'");
    }
    
    std::stringstream data;
    data << file.rdbuf();
    
    return data.str();
}


bool writeFile(const std::string& filename, const std::string& data) {
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.good()) {
        return false;
    }
    
    file << data;
    return true;
}


int main(int argc, const char * argv[]) {
    
    const std::string font_file = "/Users/machfe522/Documents/fontnik/fonts/NotoSansCJK-DemiLight.otf";
    const auto& xx = node_fontnik::Load(font_file);
   
    
    const auto& pbf = node_fontnik::Range(font_file, 0, 255);
  
    
    writeFile("/Users/machfe522/Desktop/font_0-255.pbf", pbf);

        
    
    return 0;
     
}
