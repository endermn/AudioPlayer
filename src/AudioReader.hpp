#include <fstream>
#include <cstring>


struct ID3Tag {
    char title[31];
    char artist[31];
    char album[31];
    char year[5];
};

bool extractID3Tag(const std::string& filename, ID3Tag& tag) {
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        log("Unable to open file " + filename, ERROR);
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();

    const int tag_size = 128;
    if (fileSize < tag_size) {
		log("File is too small to contain an id3 tag", ERROR);
        return false;
    }

    file.seekg(-tag_size, std::ios::end);

    char id3Header[4];
    file.read(id3Header, 3);
    id3Header[3] = '\0';

    if (std::strcmp(id3Header, "TAG") != 0) {
        log("File does not contain an ID3 tag", INFO);
        return false;
    }

    file.read(tag.title, 30);
    file.read(tag.artist, 30);
    file.read(tag.album, 30);
    file.read(tag.year, 4);

    tag.title[30] = '\0';
    tag.artist[30] = '\0';
    tag.album[30] = '\0';
    tag.year[4] = '\0';

    return true;
}