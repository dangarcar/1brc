#include <map>
#include <cstdio>
#include <string_view>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <array>

#include <iostream>
using namespace std;

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#undef TEST

//NOT USED FOR NOW
constexpr int THREADS = 1;

struct row {
    int max, min, sum, count;

    void print() const {
        auto avg = sum/count;
        printf("%d.%d/%d.%d/%d.%d", min/10, min%10, avg/10, avg%10, max/10, max%10);
    }
};

inline array<string_view, THREADS> splitInChunks(const char* data, size_t sz) {
    const auto npos = data + sz;
    const auto chunkSize = sz/THREADS;
    
    array<string_view, THREADS> chunks{};
    auto curr = data;
    for(int i=0; i<THREADS; ++i) {
        if(curr >= npos) break;

        auto end = find(curr + chunkSize, npos, '\n');
        chunks[i] = string_view(curr, end);
        curr = end + 1;
    }

    return chunks;
}

unordered_map<string, row> getTableFromChunk(string_view buffer) {
    unordered_map<string, row> m;
    m.reserve(10000);
    const auto sz = buffer.size();

    size_t curr = 0L;
    while(curr < sz) {
        auto next = buffer.find('\n', curr);
        if(next == string_view::npos) next = sz;
        string_view buf(buffer.data() + curr, next-curr);

        size_t i = 0L; 
        while(buf[i] != ';') i++;
        auto last = i;
        i++;

        auto tmp = 0;
        for(; i < buf.size(); ++i) {
            if('0' <= buf[i] && buf[i] <= '9') {
                tmp *= 10;
                tmp += buf[i] - '0';
            }
        }

        auto name = string{ buf.substr(0, last) };
        if(m.count(name)) {
            auto& p = m[name];
            p.count++;
            p.sum += tmp;
            p.min = min(p.min, tmp);
            p.max = max(p.max, tmp);
        } else {
            m.emplace(make_pair(name, row {
                tmp, tmp, tmp, 1
            }));
        }

        curr = next + 1;
    }

    return m;
}

int main(int argc, char const *argv[]) {

#ifdef TEST
    int fd = ::open("data/dummy.txt", O_RDONLY);
#else
    int fd = ::open("data/measurements.txt", O_RDONLY);
#endif
    
    if(!fd) {
        perror("Error opening data");
        exit(1);
    }

    struct stat sb;
    if(fstat(fd, &sb) == -1) {
        perror("Error getting file size");
        exit(1);
    }

    auto sz = sb.st_size;
    const char* data = (const char*) mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
    if(data == MAP_FAILED) {
        perror("Error mapping file");
        exit(1);
    }

    const auto chunks = splitInChunks(data, sz);
    unordered_map<string, row> m;
    m.reserve(10000);

    for(auto c: chunks) {
        if(!c.empty()) {
            auto batch = getTableFromChunk(c);
            printf("%lu, %lu\n", c.size(), batch.size());
            m.insert(batch.begin(), batch.end());
        }
    }

    printf("{");
    for(auto [name, r]: m) {
        printf("%s ", name.c_str());
        r.print();
        printf(", ");
    }
    printf("}");

    return 0;
}