#include "common/file.h"
#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

namespace xld::wasm {

inline u32 get_umask() {
    u32 orig_umask = umask(0);
    umask(orig_umask);
    return orig_umask;
}

template <typename Context>
static std::pair<i64, char *>
open_or_create_file(Context &ctx, std::string path, i64 filesize, i64 perm) {
    std::string tmpl = filepath(path).parent_path() / ".xld-XXXXXX";
    char *path2 = (char *)save_string(ctx, tmpl).data();

    i64 fd = mkstemp(path2);
    if (fd == -1)
        Fatal(ctx) << "cannot open " << path2 << ": " << errno_string();

    // Reuse an existing file if exists and writable because on Linux,
    // writing to an existing file is much faster than creating a fresh
    // file and writing to it.
    if (rename(path.c_str(), path2) == 0) {
        ::close(fd);
        fd = ::open(path2, O_RDWR | O_CREAT, perm);
        if (fd != -1 && !ftruncate(fd, filesize) &&
            !fchmod(fd, perm & ~get_umask()))
            return {fd, path2};

        unlink(path2);
        fd = ::open(path2, O_RDWR | O_CREAT, perm);
        if (fd == -1)
            Fatal(ctx) << "cannot open " << path2 << ": " << errno_string();
    }

    if (fchmod(fd, (perm & ~get_umask())) == -1)
        Fatal(ctx) << "fchmod failed: " << errno_string();

#ifdef __linux__
    if (fallocate(fd, 0, 0, filesize) == 0)
        return {fd, path2};
#endif

    if (ftruncate(fd, filesize) == -1)
        Fatal(ctx) << "ftruncate failed: " << errno_string();
    return {fd, path2};
}

template <typename Context>
class OutputFile {
  public:
    static std::unique_ptr<OutputFile<Context>>
    open(Context &ctx, std::string path, i64 filesize, i64 perm);

    virtual void close(Context &ctx) = 0;
    virtual ~OutputFile() = default;

    u8 *buf = nullptr;
    std::vector<u8> buf2;
    std::string path;
    i64 fd = -1;
    i64 filesize = 0;
    bool is_mmapped = false;
    bool is_unmapped = false;

  protected:
    OutputFile(std::string path, i64 filesize, bool is_mmapped)
        : path(path), filesize(filesize), is_mmapped(is_mmapped) {}
};

template <typename Context>
class MemoryMappedOutputFile : public OutputFile<Context> {
  public:
    MemoryMappedOutputFile(Context &ctx, std::string path, i64 filesize,
                           i64 perm)
        : OutputFile<Context>(path, filesize, true) {
        std::tie(this->fd, output_tmpfile) =
            open_or_create_file(ctx, path, filesize, perm);

        this->buf = (u8 *)mmap(nullptr, filesize, PROT_READ | PROT_WRITE,
                               MAP_SHARED, this->fd, 0);
        if (this->buf == MAP_FAILED)
            Fatal(ctx) << path << ": mmap failed: " << errno_string();

        // mold::output_buffer_start = this->buf;
        // mold::output_buffer_end = this->buf + filesize;
    }

    ~MemoryMappedOutputFile() {
        if (fd2 != -1)
            ::close(fd2);
    }

    void close(Context &ctx) override {
        if (!this->is_unmapped)
            munmap(this->buf, this->filesize);

        if (this->buf2.empty()) {
            ::close(this->fd);
        } else {
            FILE *out = fdopen(this->fd, "w");
            fseek(out, 0, SEEK_END);
            fwrite(&this->buf2[0], this->buf2.size(), 1, out);
            fclose(out);
        }

        // If an output file already exists, open a file and then remove it.
        // This is the fastest way to unlink a file, as it does not make the
        // system to immediately release disk blocks occupied by the file.
        fd2 = ::open(this->path.c_str(), O_RDONLY);
        if (fd2 != -1)
            unlink(this->path.c_str());

        if (rename(output_tmpfile, this->path.c_str()) == -1)
            Fatal(ctx) << this->path << ": rename failed: " << errno_string();
        output_tmpfile = nullptr;
    }

  private:
    int fd2 = -1;
};

template <typename Context>
std::unique_ptr<OutputFile<Context>>
OutputFile<Context>::open(Context &ctx, std::string path, i64 filesize,
                          i64 perm) {

    if (path.starts_with('/') && !ctx.arg.chroot.empty())
        path = ctx.arg.chroot + "/" + path_clean(path);

    OutputFile<Context> *file;
    file = new MemoryMappedOutputFile(ctx, path, filesize, perm);

#ifdef MADV_HUGEPAGE
    // Enable transparent huge page for an output memory-mapped file.
    // On Linux, it has an effect only on tmpfs mounted with `huge=advise`,
    // but it can make the linker ~10% faster. You can try it by creating
    // a tmpfs with the following commands
    //
    //  $ mkdir tmp
    //  $ sudo mount -t tmpfs -o size=2G,huge=advise none tmp
    //
    // and then specifying a path under the directory as an output file.
    madvise(file->buf, filesize, MADV_HUGEPAGE);
#endif

    // TODO: what is this for?
    // if (ctx.arg.filler != -1)
    //    memset(file->buf, ctx.arg.filler, filesize);
    return std::unique_ptr<OutputFile>(file);
}

} // namespace xld::wasm
