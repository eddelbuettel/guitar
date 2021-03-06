#include "index.h"
#include "entry_points.h"
#include "tree.h"
#include "oid.h"
#include "gexception.h"

using namespace Rcpp;

Index::Index(git_index *_ix)
{
    ix = boost::shared_ptr<git_index>(_ix, git_index_free);
}

SEXP Index::read()
{
    BEGIN_RCPP
    int result = git_index_read(ix.get(), 0);
    if (result) {
        throw GitException("reading index");
    }
    return R_NilValue;
    END_RCPP
}

SEXP Index::write()
{
    BEGIN_RCPP
    int result = git_index_write(ix.get());
    if (result) {
        throw GitException("writing index");
    }
    return R_NilValue;
    END_RCPP
}

void Index::read_tree(SEXP _tree)
{
    const git_tree *tree = Tree::from_sexp(_tree);
    int err = git_index_read_tree(ix.get(), tree);
    if (err)
        throw GitException("reading index tree");
}

Rcpp::Reference Index::write_tree()
{
    git_oid result;
    int err = git_index_write_tree(&result, ix.get());
    if (err)
        throw GitException("writing index tree");
    return OID::create(&result);
}

size_t Index::entrycount()
{
    return git_index_entrycount(ix.get());
}

void Index::clear()
{
    git_index_clear(ix.get());
}

struct entry_internal {
    git_index_entry entry;
    char path[1];
};

void Index::add(SEXP s_entry)
{
    git_index_entry *e;
    entry_internal *ei;
    Rcpp::List el(s_entry);
    if (!el.containsElementNamed("path") || !el.containsElementNamed("oid"))
	throw Rcpp::exception("index entry must contain at least a path and oid");
    std::string path = el["path"];
    int p_len = strlen(path.c_str());
    int mode = 33188; // 100644
    SEXP s_oid = el["oid"];
    const git_oid *oid = OID::from_sexp(s_oid);
    if (el.containsElementNamed("mode"))
	mode = el["mode"];
    ei = (entry_internal*) malloc(sizeof(entry_internal) + p_len + 2);
    if (!ei)
	throw Rcpp::exception("cannot allocate index entry object");
    e = &ei->entry;
    memset(e, 0, sizeof(git_index_entry));
    strcpy((char*) ei->path, path.c_str());
    e->path = ei->path;
    e->mode = mode;
    e->flags = p_len & GIT_IDXENTRY_NAMEMASK;
    if (el.containsElementNamed("uid"))
	e->uid = (int) el["uid"];
    if (el.containsElementNamed("gid"))
	e->gid = (int) el["gid"];
    e->id = *oid;
    int err = git_index_add(ix.get(), e);
    if (err)
	throw GitException("adding an index entry");
}

void Index::add_by_path(std::string path)
{
    int err = git_index_add_bypath(ix.get(), path.c_str());
    if (err)
        throw GitException("add to index by path");
}

void Index::remove_by_path(std::string path)
{
    int err = git_index_remove_bypath(ix.get(), path.c_str());
    if (err)
      throw GitException("remove form index by path");
}

void Index::remove_directory(std::string path, int stage)
{
    int err = git_index_remove_directory(ix.get(), path.c_str(), stage);
    if (err)
        throw GitException("remove directory from index");
}

namespace IndexEntry {
    Rcpp::Datetime index_time(const git_index_time *time) {
	double t = (double) time->seconds;
	double t2 = (double) time->nanoseconds;
	t += t2 / 1000000000.0;
	return Rcpp::Datetime(t);
    }
    Rcpp::List create(const git_index_entry *entry) {
        SEXP oid = OID::create(&entry->id);
        return Rcpp::List::create(Rcpp::Named("ctime") = index_time(&entry->ctime),
                                  Rcpp::Named("mtime") = index_time(&entry->mtime),
                                  Rcpp::Named("dev") = entry->dev,
                                  Rcpp::Named("ino") = entry->ino,
                                  Rcpp::Named("mode") = entry->mode,
                                  Rcpp::Named("uid") = entry->uid,
                                  Rcpp::Named("gid") = entry->gid,
                                  Rcpp::Named("file_size") = (double) entry->file_size,
                                  Rcpp::Named("oid") = oid,
                                  Rcpp::Named("flags") = entry->flags,
                                  Rcpp::Named("flags_extended") = entry->flags_extended,
                                  Rcpp::Named("path") = std::string(entry->path));
    }
};

Rcpp::List Index::get_by_index(size_t n) {
    const git_index_entry *result = git_index_get_byindex(ix.get(), n);
    if (result == NULL)
        throw GitException("get by index");
    return IndexEntry::create(result);
}

Rcpp::List Index::get_by_path(std::string path, int stage) {
    const git_index_entry *result = git_index_get_bypath(ix.get(), path.c_str(), stage);
    if (result == NULL)
        throw GitException("get by path");
    return IndexEntry::create(result);
}

RCPP_MODULE(guitar_index) {
    class_<Index>("Index")
        .method("read", &Index::read)
        .method("write", &Index::write)
        .method("write_tree", &Index::write_tree)
        .method("entrycount", &Index::entrycount)
        .method("clear", &Index::clear)
	.method("add", &Index::add)
        .method("add_by_path", &Index::add_by_path)
        .method("remove_by_path", &Index::remove_by_path)
        .method("remove_directory", &Index::remove_directory)
        .method("read_tree", &Index::read_tree)
        .method("get_by_index", &Index::get_by_index)
        .method("get_by_path", &Index::get_by_path)
        ;
}
