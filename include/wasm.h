#pragma once

#include "common/mmap.h"
#include "section.h"
#include "xld.h"

namespace xld::wasm {

struct WASM32 {};
struct WASM64 {};

template <typename E>
class InputFile {
  public:
    InputFile(Context<E> &ctx, MappedFile *mf);

    virtual ~InputFile() = default;

    // template <typename T>
    // std::span<T> get_data(Context<E> &ctx, i64 idx);

    // virtual void resolve_symbols(Context<E> &ctx) = 0;
    // void clear_symbols();

    MappedFile *mf = nullptr;

    std::string filename;
};

template <typename E>
class ObjectFile : public InputFile<E> {
  public:
    static ObjectFile<E> *create(Context<E> &ctx, MappedFile *mf);

    void parse(Context<E> &ctx);

  private:
    ObjectFile(Context<E> &ctx, MappedFile *mf);

    std::vector<std::unique_ptr<InputSection<E>>> sections;
};

} // namespace xld::wasm
