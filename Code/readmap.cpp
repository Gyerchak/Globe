// readmap.cpp
// Compile: g++ -std=c++20 -static Code/readmap.cpp -o Executables/readmap.exe -lz
// Reads raw raster (.bin) and GeoTIFF files from Input/, outputs metadata,
// highest, lowest, highest non-zero, lowest non-zero pixel values,
// presence of zero, all pixel values (original order), and precision
// displayed as a normal decimal number with reciprocal fraction.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>          // for std::fixed, std::setprecision
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <set>
#include <regex>
#include <limits>
#include <cmath>            // for std::round
#include <zlib.h>

namespace fs = std::filesystem;

// ======================== Byte order helpers ========================
static inline bool isLittleEndianNative() {
    uint16_t v = 1;
    return *reinterpret_cast<uint8_t*>(&v) == 1;
}
static uint16_t read16(const uint8_t* buf, bool fileIsLittle) {
    uint16_t val; std::memcpy(&val, buf, 2);
    if (fileIsLittle != isLittleEndianNative()) val = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
    return val;
}
static uint32_t read32(const uint8_t* buf, bool fileIsLittle) {
    uint32_t val; std::memcpy(&val, buf, 4);
    if (fileIsLittle != isLittleEndianNative()) val = ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
              ((val >> 8) & 0xFF00) | ((val >> 24) & 0xFF);
    return val;
}
static uint64_t read64(const uint8_t* buf, bool fileIsLittle) {
    uint64_t val; std::memcpy(&val, buf, 8);
    if (fileIsLittle != isLittleEndianNative()) val = ((val & 0xFFULL) << 56) | ((val & 0xFF00ULL) << 40) |
              ((val & 0xFF0000ULL) << 24) | ((val & 0xFF000000ULL) << 8) |
              ((val >> 8) & 0xFF000000ULL) | ((val >> 24) & 0xFF0000ULL) |
              ((val >> 40) & 0xFF00ULL) | ((val >> 56) & 0xFFULL);
    return val;
}

// ======================== TIFF tag constants ========================
enum TiffTag : uint16_t {
    TAG_IMAGE_WIDTH = 256, TAG_IMAGE_LENGTH = 257, TAG_BITS_PER_SAMPLE = 258,
    TAG_COMPRESSION = 259, TAG_STRIP_OFFSETS = 273, TAG_ROWS_PER_STRIP = 278,
    TAG_STRIP_BYTE_COUNTS = 279, TAG_TILE_OFFSETS = 324, TAG_TILE_BYTE_COUNTS = 325,
    TAG_TILE_WIDTH = 322, TAG_TILE_LENGTH = 323, TAG_SAMPLES_PER_PIXEL = 277,
    TAG_PLANAR_CONFIG = 284, TAG_SAMPLE_FORMAT = 339, TAG_PREDICTOR = 317
};
enum TiffType : uint16_t { TIFF_BYTE=1, TIFF_ASCII=2, TIFF_SHORT=3, TIFF_LONG=4, TIFF_RATIONAL=5,
    TIFF_SBYTE=6, TIFF_UNDEFINED=7, TIFF_SSHORT=8, TIFF_SLONG=9, TIFF_SRATIONAL=10,
    TIFF_FLOAT=11, TIFF_DOUBLE=12 };
enum TiffCompression : uint16_t { COMP_NONE=1, COMP_LZW=5, COMP_DEFLATE=8, COMP_PACKBITS=32773 };

// ======================== TIFF info struct ========================
struct GeoTiffInfo {
    uint32_t width=0, height=0; uint16_t bitsPerSample=0, sampleFormat=1, samplesPerPixel=1,
              compression=COMP_NONE, predictor=1;
    uint32_t rowsPerStrip=0, tileWidth=0, tileHeight=0;
    bool littleEndian=true, isTiled=false;
    std::vector<uint64_t> stripOffsets, stripByteCounts, tileOffsets, tileByteCounts;
    uint64_t bytesPerRow=0; uint16_t bytesPerSample=0; std::string error;
};

// ======================== IFD parser ========================
static bool parseTiffIFD(std::ifstream& f, uint32_t ifdOffset, GeoTiffInfo& info) {
    f.seekg(ifdOffset); if (!f) return false;
    uint8_t numBuf[2]; f.read((char*)numBuf,2);
    uint16_t numEntries = read16(numBuf, info.littleEndian);
    info.rowsPerStrip = info.height;
    auto readArray = [&](uint32_t off, uint16_t typ, uint32_t cnt)->std::vector<uint64_t>{
        std::vector<uint64_t> res; size_t es = (typ==TIFF_SHORT)?2:4;
        auto cur = f.tellg(); f.seekg(off);
        for(uint32_t j=0;j<cnt;++j){ uint8_t b[4]={0}; f.read((char*)b,es);
            res.push_back(typ==TIFF_SHORT?read16(b,info.littleEndian):read32(b,info.littleEndian)); }
        f.seekg(cur); return res;
    };
    for(int i=0;i<numEntries;++i){
        uint8_t ent[12]; f.read((char*)ent,12);
        uint16_t tag=read16(ent,info.littleEndian), typ=read16(ent+2,info.littleEndian);
        uint32_t cnt=read32(ent+4,info.littleEndian), v32=read32(ent+8,info.littleEndian);
        switch(tag){
            case TAG_IMAGE_WIDTH: if(typ==TIFF_LONG||typ==TIFF_SHORT) info.width=v32; break;
            case TAG_IMAGE_LENGTH: if(typ==TIFF_LONG||typ==TIFF_SHORT) info.height=v32; break;
            case TAG_BITS_PER_SAMPLE: if(cnt==1) info.bitsPerSample=v32; else { auto a=readArray(v32,typ,cnt); if(!a.empty()) info.bitsPerSample=a[0]; } break;
            case TAG_COMPRESSION: info.compression=v32; break;
            case TAG_SAMPLES_PER_PIXEL: if(typ==TIFF_SHORT) info.samplesPerPixel=v32; break;
            case TAG_SAMPLE_FORMAT: if(cnt==1) info.sampleFormat=v32; else { auto a=readArray(v32,typ,cnt); if(!a.empty()) info.sampleFormat=a[0]; } break;
            case TAG_PREDICTOR: info.predictor=v32; break;
            case TAG_STRIP_OFFSETS: if(typ==TIFF_LONG||typ==TIFF_SHORT){if(cnt==1) info.stripOffsets.push_back(v32); else { auto a=readArray(v32,typ,cnt); info.stripOffsets.insert(info.stripOffsets.end(),a.begin(),a.end()); }} break;
            case TAG_STRIP_BYTE_COUNTS: if(typ==TIFF_LONG||typ==TIFF_SHORT){if(cnt==1) info.stripByteCounts.push_back(v32); else { auto a=readArray(v32,typ,cnt); info.stripByteCounts.insert(info.stripByteCounts.end(),a.begin(),a.end()); }} break;
            case TAG_TILE_OFFSETS: info.isTiled=true; if(typ==TIFF_LONG||typ==TIFF_SHORT){if(cnt==1) info.tileOffsets.push_back(v32); else { auto a=readArray(v32,typ,cnt); info.tileOffsets.insert(info.tileOffsets.end(),a.begin(),a.end()); }} break;
            case TAG_TILE_BYTE_COUNTS: info.isTiled=true; if(typ==TIFF_LONG||typ==TIFF_SHORT){if(cnt==1) info.tileByteCounts.push_back(v32); else { auto a=readArray(v32,typ,cnt); info.tileByteCounts.insert(info.tileByteCounts.end(),a.begin(),a.end()); }} break;
            case TAG_TILE_WIDTH: info.tileWidth=v32; break;
            case TAG_TILE_LENGTH: info.tileHeight=v32; break;
            case TAG_ROWS_PER_STRIP: info.rowsPerStrip=v32; break;
            case TAG_PLANAR_CONFIG: if(v32!=1){ info.error="Planar config not supported"; return false; } break;
        }
    }
    if(!info.width||!info.height){info.error="Missing dimensions";return false;}
    if(info.samplesPerPixel!=1){info.error="Only grayscale supported";return false;}
    if(!info.bitsPerSample){info.error="Missing BitsPerSample";return false;}
    info.bytesPerSample = (info.bitsPerSample+7)/8;
    info.bytesPerRow = (uint64_t)info.width * info.bytesPerSample;
    if(info.isTiled){
        if(!info.tileWidth||!info.tileHeight){info.error="Missing tile dims";return false;}
        if(info.tileOffsets.empty()||info.tileByteCounts.empty()){info.error="No tile offsets";return false;}
        uint32_t across=(info.width+info.tileWidth-1)/info.tileWidth, down=(info.height+info.tileHeight-1)/info.tileHeight;
        if(info.tileOffsets.size()!=across*down){info.error="Mismatched tile count";return false;}
    }else{
        if(info.stripOffsets.empty()||info.stripByteCounts.empty()){info.error="No strip offsets";return false;}
        if(!info.rowsPerStrip) info.rowsPerStrip = info.height;
        if(info.stripOffsets.size()!=(info.height+info.rowsPerStrip-1)/info.rowsPerStrip){info.error="Mismatched strip count";return false;}
    }
    return true;
}

// ======================== Decompression ========================
static std::vector<uint8_t> unpackPackBits(const uint8_t* d, size_t csize, size_t usize){
    std::vector<uint8_t> out; out.reserve(usize); size_t i=0;
    while(i<csize && out.size()<usize){ int8_t n=d[i++];
        if(n>=0){ size_t cnt=n+1; if(i+cnt>csize) break; out.insert(out.end(),d+i,d+i+cnt); i+=cnt; }
        else if(n!=-128){ size_t cnt=-n+1; if(i>=csize) break; out.insert(out.end(),cnt,d[i++]); }
    } return out;
}
static std::vector<uint8_t> decodeLZW(const uint8_t* d, size_t csize, size_t usize){
    std::vector<uint8_t> out; out.reserve(usize); size_t bitPos=0;
    auto rb=[&](int n){ int r=0; for(int i=0;i<n;++i){ size_t bi=bitPos/8,bo=7-(bitPos%8); if(bi>=csize)return-1;
        if(d[bi]&(1<<bo)) r|=1; if(i!=n-1) r<<=1; ++bitPos; } return r; };
    const int CL=256,EOI=257,FIRST=258; int bits=9,max=(1<<bits)-1,next=FIRST;
    std::vector<std::vector<uint8_t>> tab(4096); for(int i=0;i<256;++i) tab[i].push_back((uint8_t)i);
    int old=rb(bits); if(old<0||old>=256)return{}; out.push_back((uint8_t)old); uint8_t fc=(uint8_t)old;
    while(out.size()<usize){ int code=rb(bits); if(code==EOI||code<0)break; if(code==CL){ tab.resize(4096);
        for(int i=0;i<256;++i){tab[i].clear(); tab[i].push_back((uint8_t)i);} next=FIRST; bits=9; max=(1<<bits)-1;
        old=rb(bits); if(old<0||old>=256)break; out.push_back((uint8_t)old); fc=(uint8_t)old; continue; }
        std::vector<uint8_t> cur; if(code<next) cur=tab[code]; else if(code==next){cur=tab[old]; cur.push_back(fc);}
        else break; out.insert(out.end(),cur.begin(),cur.end()); fc=cur[0];
        if(next<4096){tab[next]=tab[old]; tab[next].push_back(fc); ++next;} old=code;
        if(next>max && bits<12){++bits; max=(1<<bits)-1;} } return out;
}
static std::vector<uint8_t> decompressDeflate(const uint8_t* d, size_t csize, size_t usize){
    std::vector<uint8_t> out(usize); z_stream s={};
    s.next_in=const_cast<uint8_t*>(d); s.avail_in=(uInt)csize; s.next_out=out.data(); s.avail_out=(uInt)usize;
    int r=inflateInit2(&s,-MAX_WBITS); if(r==Z_OK){ r=inflate(&s,Z_FINISH); inflateEnd(&s); if(r==Z_STREAM_END){out.resize(s.total_out); return out;}}
    s={}; s.next_in=const_cast<uint8_t*>(d); s.avail_in=(uInt)csize; s.next_out=out.data(); s.avail_out=(uInt)usize;
    r=inflateInit2(&s,MAX_WBITS); if(r==Z_OK){ r=inflate(&s,Z_FINISH); inflateEnd(&s); if(r==Z_STREAM_END){out.resize(s.total_out); return out;}}
    return{};
}
static std::vector<uint8_t> decompressData(const uint8_t* cdata, size_t csize, size_t usize, uint16_t comp){
    if(comp==COMP_NONE) return {cdata, cdata+std::min(csize, usize)};
    if(comp==COMP_PACKBITS) return unpackPackBits(cdata,csize,usize);
    if(comp==COMP_LZW) return decodeLZW(cdata,csize,usize);
    if(comp==COMP_DEFLATE) return decompressDeflate(cdata,csize,usize);
    return{};
}

// ======================== Predictor undo ========================
static void applyPredictor(std::vector<uint8_t>& raw, uint64_t bpr, uint16_t bps, uint32_t rows, uint16_t pred){
    if(pred==1||raw.empty()||!bps) return;
    for(uint32_t row=0;row<rows;++row){ size_t rs=row*bpr;
        for(uint32_t col=1;col<(bpr/bps);++col){ size_t off=rs+col*bps; uint64_t p=0,c=0;
            std::memcpy(&p,&raw[rs+(col-1)*bps],bps); std::memcpy(&c,&raw[off],bps);
            if(bps==1) raw[off]=uint8_t(p+c); else if(bps==2){ uint16_t v=uint16_t(p+c); std::memcpy(&raw[off],&v,2); }
            else if(bps==4){ uint32_t v=uint32_t(p+c); std::memcpy(&raw[off],&v,4); }
            else if(bps==8){ uint64_t v=p+c; std::memcpy(&raw[off],&v,8); } }
    }
}

// ======================== Coordinate parser ========================
static std::string parseCopernicusCoordinates(const std::string& stem){
    std::regex pattern(R"(([NS])(\d{2})_(\d{2})_([EW])(\d{3})_(\d{2})_DEM$)");
    std::smatch match;
    if(std::regex_search(stem, match, pattern)){
        char latH = match[1].str()[0]; int latD = std::stoi(match[2].str()); int latM = std::stoi(match[3].str());
        char lonH = match[4].str()[0]; int lonD = std::stoi(match[5].str()); int lonM = std::stoi(match[6].str());
        double lat = latD + latM/60.0, lon = lonD + lonM/60.0;
        char buf[100]; snprintf(buf,sizeof(buf),"%.2f %c, %.2f %c", lat, latH, lon, lonH);
        return buf;
    }
    return "Unknown";
}

// ======================== Read all pixels + stats ========================
struct PixelStats {
    double highest = -std::numeric_limits<double>::infinity();
    double lowest = std::numeric_limits<double>::infinity();
    double highestNonZero = -std::numeric_limits<double>::infinity();
    double lowestNonZero = std::numeric_limits<double>::infinity();
    bool hasZero = false;
};

static std::vector<double> readAllTiffPixels(const std::string& filename, const GeoTiffInfo& info, PixelStats& stats) {
    std::vector<double> allPixels;
    std::ifstream f(filename, std::ios::binary);
    if (!f) return allPixels;

    for (size_t chunkIdx = 0; chunkIdx < (info.isTiled ? info.tileOffsets.size() : info.stripOffsets.size()); ++chunkIdx) {
        uint64_t offset = info.isTiled ? info.tileOffsets[chunkIdx] : info.stripOffsets[chunkIdx];
        uint64_t byteCount = info.isTiled ? info.tileByteCounts[chunkIdx] : info.stripByteCounts[chunkIdx];
        size_t expectedSize;
        uint32_t rowsInChunk;
        if (info.isTiled) {
            expectedSize = (size_t)info.tileWidth * info.tileHeight * info.bytesPerSample;
            rowsInChunk = info.tileHeight;
        } else {
            uint32_t stripRows = std::min(info.rowsPerStrip, info.height - (uint32_t)(chunkIdx * info.rowsPerStrip));
            expectedSize = (size_t)stripRows * info.bytesPerRow;
            rowsInChunk = stripRows;
        }
        f.seekg(offset);
        std::vector<uint8_t> comp(byteCount);
        f.read((char*)comp.data(), byteCount);
        size_t rd = f.gcount();
        comp.resize(rd);
        std::vector<uint8_t> raw = decompressData(comp.data(), rd, expectedSize, info.compression);
        if (raw.empty()) { allPixels.clear(); return allPixels; }
        applyPredictor(raw, info.bytesPerRow, info.bytesPerSample, rowsInChunk, info.predictor);

        size_t numPixels = raw.size() / info.bytesPerSample;
        for (size_t i = 0; i < numPixels; ++i) {
            const uint8_t* p = raw.data() + i * info.bytesPerSample;
            double val = 0.0;
            if (info.sampleFormat == 3) {
                if (info.bytesPerSample == 4) { uint32_t v; std::memcpy(&v, p, 4); float f; std::memcpy(&f, &v, 4); val = f; }
                else if (info.bytesPerSample == 8) { std::memcpy(&val, p, 8); }
            } else {
                if (info.bytesPerSample == 1) val = (info.sampleFormat == 2) ? (int8_t)*p : *p;
                else if (info.bytesPerSample == 2) { uint16_t v = read16(p, info.littleEndian); val = (info.sampleFormat == 2) ? (int16_t)v : v; }
                else if (info.bytesPerSample == 4) { uint32_t v = read32(p, info.littleEndian); val = (info.sampleFormat == 2) ? (int32_t)v : v; }
            }
            allPixels.push_back(val);

            // Update statistics
            if (val > stats.highest) stats.highest = val;
            if (val < stats.lowest) stats.lowest = val;
            if (val == 0.0) {
                stats.hasZero = true;
            } else {
                if (val > stats.highestNonZero) stats.highestNonZero = val;
                if (val < stats.lowestNonZero) stats.lowestNonZero = val;
            }
        }
    }
    return allPixels;
}

// ======================== Compute smallest non-zero difference ========================
static double computePrecision(const std::vector<double>& pixels) {
    if (pixels.size() < 2) return -1.0;
    std::vector<double> sorted = pixels;  // copy to preserve original order
    std::sort(sorted.begin(), sorted.end());
    double minDiff = std::numeric_limits<double>::max();
    for (size_t i = 1; i < sorted.size(); ++i) {
        double diff = sorted[i] - sorted[i-1];
        if (diff > 0.0 && diff < minDiff) {
            minDiff = diff;
        }
    }
    return (minDiff == std::numeric_limits<double>::max()) ? -1.0 : minDiff;
}

// ======================== Raw binary (unchanged) ========================
static constexpr size_t WIDTH = 65536, HEIGHT = 32768, TOTAL_PIXELS = WIDTH * HEIGHT;
struct RawFileInfo { size_t bitsPerPixel; std::vector<uintmax_t> first10; std::string error; };
static bool processRawBinary(const std::string& path, RawFileInfo& info) {
    uintmax_t sz = fs::file_size(path);
    if (sz % TOTAL_PIXELS != 0) { info.error = "size not multiple of " + std::to_string(TOTAL_PIXELS); return false; }
    size_t bpp = sz / TOTAL_PIXELS * 8;
    if (bpp != 8 && bpp != 16 && bpp != 32) { info.error = "unsupported bpp: " + std::to_string(bpp); return false; }
    info.bitsPerPixel = bpp;
    std::ifstream fin(path, std::ios::binary);
    if (!fin) { info.error = "cannot open"; return false; }
    for (int i = 0; i < 10; ++i) {
        uintmax_t v = 0;
        if (bpp == 8) { uint8_t t; if (fin.read((char*)&t, 1)) v = t; }
        else if (bpp == 16) { uint16_t t; if (fin.read((char*)&t, 2)) v = t; }
        else { uint32_t t; if (fin.read((char*)&t, 4)) v = t; }
        info.first10.push_back(v);
        if (!fin) break;
    }
    return true;
}

// ======================== Main ========================
int main() {
    fs::path exeDir = fs::current_path();
    if (exeDir.filename() == "Executables") exeDir = exeDir.parent_path();
    fs::path inputDir = exeDir / "Input", outputDir = exeDir / "Output";
    if (!fs::exists(inputDir) || !fs::is_directory(inputDir)) {
        std::cerr << "ERROR: Input folder not found: " << inputDir << '\n';
        std::cout << "Press Enter to exit..."; std::cin.get(); return 1;
    }
    fs::create_directories(outputDir);
    std::set<std::string> tiffExts = {".tif", ".tiff"};
    std::cout << "Scanning: " << inputDir << '\n';

    for (const auto& entry : fs::directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;
        fs::path inPath = entry.path();
        std::string stem = inPath.stem().string();
        fs::path outPath = outputDir / (stem + ".txt");
        std::string ext = inPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        bool isTiff = tiffExts.count(ext);

        if (isTiff) {
            GeoTiffInfo info;
            std::ifstream f(inPath, std::ios::binary);
            if (!f) { std::cerr << "Skipping " << inPath.filename() << " - cannot open\n"; continue; }
            uint8_t hdr[8]; f.read((char*)hdr, 8);
            if (!f || (hdr[0] != 'I' && hdr[0] != 'M') || hdr[1] != hdr[0]) {
                std::cerr << "Skipping " << inPath.filename() << " - not TIFF\n"; continue;
            }
            info.littleEndian = (hdr[0] == 'I');
            if (read16(hdr + 2, info.littleEndian) != 42) {
                std::cerr << "Skipping " << inPath.filename() << " - bad magic\n"; continue;
            }
            uint32_t ifdOff = read32(hdr + 4, info.littleEndian);
            if (!parseTiffIFD(f, ifdOff, info)) {
                std::cerr << "Skipping " << inPath.filename() << " - " << info.error << '\n'; continue;
            }

            // Read all pixels and compute stats
            PixelStats stats;
            std::vector<double> pixels = readAllTiffPixels(inPath.string(), info, stats);
            if (pixels.empty()) {
                std::cerr << "Skipping " << inPath.filename() << " - could not read pixels\n";
                continue;
            }

            // Compute precision (smallest non-zero difference) on a copy
            double precision = computePrecision(pixels);

            // Write output file – use fixed decimal for stats
            std::ofstream fout(outPath);
            fout << "Map coordinates (read from file name): " << parseCopernicusCoordinates(stem) << "\n";
            fout << "Bits per pixel: " << info.bitsPerSample << "\n";
            fout << "Data type: " << (info.sampleFormat == 1 ? "unsigned int" : info.sampleFormat == 2 ? "signed int" : "float") << "\n";
            fout << "Dimensions: " << info.width << "x" << info.height << "\n";
            fout << "Compression: " << (info.compression == 1 ? "none" : info.compression == 5 ? "LZW" : info.compression == 8 ? "Deflate" : info.compression == 32773 ? "PackBits" : std::to_string(info.compression)) << "\n";
            fout << "Predictor: " << info.predictor << "\n";
            if (info.isTiled) fout << "Tiled: " << info.tileWidth << "x" << info.tileHeight << "\n";
            else fout << "Rows per strip: " << info.rowsPerStrip << "\n";

            // Fixed precision for all floating-point stats
            fout << std::fixed << std::setprecision(10);
            fout << "Highest pixel value: " << stats.highest << "\n";
            fout << "Lowest pixel value: " << stats.lowest << "\n";
            fout << "Highest non-zero pixel value: ";
            if (stats.highestNonZero == -std::numeric_limits<double>::infinity()) fout << "N/A";
            else fout << stats.highestNonZero;
            fout << "\n";
            fout << "Lowest non-zero pixel value: ";
            if (stats.lowestNonZero == std::numeric_limits<double>::infinity()) fout << "N/A";
            else fout << stats.lowestNonZero;
            fout << "\n";
            fout << "Zero value present: " << (stats.hasZero ? "Yes" : "No") << "\n";

            fout << "Precision (smallest non-zero difference between distinct values): ";
            if (precision < 0) {
                fout << "N/A";
            } else {
                fout << precision;
                double recip = 1.0 / precision;
                long long denom = static_cast<long long>(std::round(recip));
                fout << " (1/" << denom << ")";
            }
            fout << "\n";

            // Reset to default floating-point formatting for the pixel list
            fout << std::defaultfloat << std::setprecision(6);
            fout << "All pixel values:\n";
            for (size_t i = 0; i < pixels.size(); ++i) {
                fout << pixels[i];
                if (i < pixels.size() - 1) fout << ' ';
            }
            fout << '\n';

            std::cout << "Created " << outPath.filename() << '\n';
        } else {
            // Raw binary file
            RawFileInfo raw;
            if (!processRawBinary(inPath.string(), raw)) {
                std::cerr << "Skipping " << inPath.filename() << " - " << raw.error << '\n';
                continue;
            }
            std::ofstream fout(outPath);
            fout << "Map coordinates (read from file name): Unknown\n";
            fout << "Bits per pixel: " << raw.bitsPerPixel << "\n";
            fout << "All pixel values (first 10):\n";
            for (size_t i = 0; i < raw.first10.size(); ++i) {
                fout << raw.first10[i];
                if (i < raw.first10.size() - 1) fout << ' ';
            }
            fout << '\n';
            std::cout << "Created " << outPath.filename() << '\n';
        }
    }
    std::cout << "\nDone. Press Enter to exit...";
    std::cin.get();
    return 0;
}