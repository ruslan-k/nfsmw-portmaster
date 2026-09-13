#include "fmod_trace.h"

#include <stddef.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if UINTPTR_MAX != UINT32_MAX
#error "The NFS MW FMOD trace bridge requires the 32-bit ARM guest runtime"
#endif

#define NFSMW_GUEST_ABI __attribute__((pcs("aapcs")))
#define FMOD_MEMORY_AB_DEFAULT_PATH \
    "/published/sounds/music/loading_01.mp3"

typedef int32_t fmod_result;

typedef fmod_result (NFSMW_GUEST_ABI *fmod_file_open_callback)(
    const char *name, int unicode, uint32_t *filesize,
    void **handle, void **userdata);
typedef fmod_result (NFSMW_GUEST_ABI *fmod_file_close_callback)(
    void *handle, void *userdata);
typedef fmod_result (NFSMW_GUEST_ABI *fmod_file_read_callback)(
    void *handle, void *buffer, unsigned int size,
    unsigned int *read_size, void *userdata);
typedef fmod_result (NFSMW_GUEST_ABI *fmod_file_seek_callback)(
    void *handle, uint32_t position, void *userdata);
typedef fmod_result (NFSMW_GUEST_ABI *fmod_file_async_read_callback)(
    void *information, void *userdata);
typedef fmod_result (NFSMW_GUEST_ABI *fmod_file_async_cancel_callback)(
    void *handle, void *userdata);

typedef fmod_result (NFSMW_GUEST_ABI *fmod_create_sound_function)(
    void *system, const char *name_or_data, uint32_t mode,
    void *extra_information, void **sound);
typedef fmod_result (NFSMW_GUEST_ABI *fmod_get_version_function)(
    void *system, uint32_t *version);
typedef fmod_result (NFSMW_GUEST_ABI *fmod_set_file_system_function)(
    void *system,
    fmod_file_open_callback user_open,
    fmod_file_close_callback user_close,
    fmod_file_read_callback user_read,
    fmod_file_seek_callback user_seek,
    fmod_file_async_read_callback user_async_read,
    fmod_file_async_cancel_callback user_async_cancel,
    int block_align);

/* Exact FMOD Ex 4.40.06 ARM32 FMOD_ASYNCREADINFO; read-only here. */
struct fmod_async_read_info_44006 {
    void *handle;
    uint32_t offset;
    uint32_t size_bytes;
    int32_t priority;
    void *buffer;
    uint32_t bytesread;
    int32_t completion_result;
    void *userdata;
};

enum {
    TRACKED_FILE_CAPACITY = 64,
    TRACKED_PATH_CAPACITY = 384,
    FMOD_44006_ASYNCINFO_SIZE = 32U,
    FMOD_44006_EXINFO_SIZE = 136U,
    FMOD_MEMORY_AB_EXPECTED_SIZE = 1252717U,
    FMOD_MEMORY_AB_MAX_SIZE = 16U * 1024U * 1024U,
    FMOD_MEMORY_READ_CHUNK = 65536U,
    FMOD_FILE_EOF_VALUE = 22,
    FMOD_HARDWARE_VALUE = 0x00000020U,
    FMOD_SOFTWARE_VALUE = 0x00000040U,
    FMOD_CREATESTREAM_VALUE = 0x00000080U,
    FMOD_OPENMEMORY_VALUE = 0x00000800U,
    FMOD_OPENMEMORY_POINT_VALUE = 0x10000000U,
    FMOD_MEMORY_MODE = 0x000008c0U
};

_Static_assert(sizeof(struct fmod_async_read_info_44006) ==
                   FMOD_44006_ASYNCINFO_SIZE,
               "FMOD 4.40.06 async info size mismatch");
_Static_assert(offsetof(struct fmod_async_read_info_44006, buffer) == 0x10,
               "FMOD async info buffer offset mismatch");
_Static_assert(offsetof(struct fmod_async_read_info_44006, bytesread) == 0x14,
               "FMOD async info bytesread offset mismatch");
_Static_assert(offsetof(struct fmod_async_read_info_44006,
                        completion_result) == 0x18,
               "FMOD async info result offset mismatch");
_Static_assert(offsetof(struct fmod_async_read_info_44006, userdata) == 0x1c,
               "FMOD async info userdata offset mismatch");

struct tracked_file {
    void *handle;
    char path[TRACKED_PATH_CAPACITY];
};

/* Exact FMOD Ex 4.40.06 ARM32 layout (FMOD_VERSION 0x00044006). */
struct fmod_44006_exinfo_arm32 {
    int32_t cbsize;
    uint32_t length;
    uint32_t fileoffset;
    int32_t numchannels;
    int32_t defaultfrequency;
    int32_t format;
    uint32_t decodebuffersize;
    int32_t initialsubsound;
    int32_t numsubsounds;
    uint32_t inclusionlist;
    int32_t inclusionlistnum;
    uint32_t pcmreadcallback;
    uint32_t pcmsetposcallback;
    uint32_t nonblockcallback;
    uint32_t dlsname;
    uint32_t encryptionkey;
    int32_t maxpolyphony;
    uint32_t userdata;
    int32_t suggestedsoundtype;
    uint32_t useropen;
    uint32_t userclose;
    uint32_t userread;
    uint32_t userseek;
    uint32_t userasyncread;
    uint32_t userasynccancel;
    int32_t speakermap;
    uint32_t initialsoundgroup;
    uint32_t initialseekposition;
    uint32_t initialseekpostype;
    int32_t ignoresetfilesystem;
    int32_t cddaforceaspi;
    uint32_t audioqueuepolicy;
    uint32_t minmidigranularity;
    int32_t nonblockthreadid;
};

_Static_assert(sizeof(struct fmod_44006_exinfo_arm32) ==
                   FMOD_44006_EXINFO_SIZE,
               "FMOD 4.40.06 exinfo size mismatch");
_Static_assert(offsetof(struct fmod_44006_exinfo_arm32, length) == 0x04,
               "FMOD exinfo length offset mismatch");
_Static_assert(offsetof(struct fmod_44006_exinfo_arm32,
                        suggestedsoundtype) == 0x48,
               "FMOD exinfo suggested type offset mismatch");
_Static_assert(offsetof(struct fmod_44006_exinfo_arm32, useropen) == 0x4c,
               "FMOD exinfo useropen offset mismatch");
_Static_assert(offsetof(struct fmod_44006_exinfo_arm32,
                        initialseekposition) == 0x6c,
               "FMOD exinfo seek offset mismatch");
_Static_assert(offsetof(struct fmod_44006_exinfo_arm32,
                        ignoresetfilesystem) == 0x74,
               "FMOD exinfo filesystem offset mismatch");
_Static_assert(offsetof(struct fmod_44006_exinfo_arm32,
                        nonblockthreadid) == 0x84,
               "FMOD exinfo tail mismatch");

enum {
    FMOD_MEMORY_AB_ASSET_CAPACITY = 32U
};

struct mp3_memory_ab_asset {
    char path[TRACKED_PATH_CAPACITY];
    void *data;
    struct fmod_44006_exinfo_arm32 *exinfo;
    uint32_t length;
    int loaded;
    int loading;
    int attempted;
    int failed;
};

static const char create_sound_symbol[] =
    "_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE";
static const char create_stream_symbol[] =
    "_ZN4FMOD6System12createStreamEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE";
static const char get_version_symbol[] =
    "_ZN4FMOD6System10getVersionEPj";
static const char set_file_system_symbol[] =
    "_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_"
    "EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_"
    "EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i";

static fmod_create_sound_function original_create_sound;
static fmod_create_sound_function original_create_stream;
static fmod_get_version_function original_get_version;
static fmod_set_file_system_function original_set_file_system;

static fmod_file_open_callback original_file_open;
static fmod_file_close_callback original_file_close;
static fmod_file_read_callback original_file_read;
static fmod_file_seek_callback original_file_seek;
static fmod_file_async_read_callback original_file_async_read;
static fmod_file_async_cancel_callback original_file_async_cancel;

static pthread_mutex_t trace_lock = PTHREAD_MUTEX_INITIALIZER;
static struct tracked_file tracked_files[TRACKED_FILE_CAPACITY];
static unsigned int create_sound_calls;
static unsigned int create_stream_calls;
static unsigned int file_open_calls;
static unsigned int file_close_calls;
static unsigned int file_read_calls;
static unsigned int file_seek_calls;
static unsigned int file_async_read_calls;
static unsigned int file_async_cancel_calls;
static int trace_enabled;
static int version_logged;
static struct mp3_memory_ab_asset memory_ab_assets[FMOD_MEMORY_AB_ASSET_CAPACITY];
static unsigned int memory_ab_asset_count;

static unsigned int next_counter(unsigned int *counter)
{
    unsigned int value;

    (void)pthread_mutex_lock(&trace_lock);
    *counter += 1U;
    value = *counter;
    (void)pthread_mutex_unlock(&trace_lock);
    return value;
}

static int sampled(unsigned int call)
{
    return call <= 64U || call % 512U == 0U;
}

static void track_file(void *handle, const char *path)
{
    size_t index;
    size_t empty = TRACKED_FILE_CAPACITY;

    if (handle == NULL) return;
    (void)pthread_mutex_lock(&trace_lock);
    for (index = 0U; index < TRACKED_FILE_CAPACITY; ++index) {
        if (tracked_files[index].handle == handle) {
            empty = index;
            break;
        }
        if (empty == TRACKED_FILE_CAPACITY &&
            tracked_files[index].handle == NULL) {
            empty = index;
        }
    }
    if (empty != TRACKED_FILE_CAPACITY) {
        tracked_files[empty].handle = handle;
        (void)snprintf(tracked_files[empty].path,
                       sizeof(tracked_files[empty].path), "%s",
                       path != NULL ? path : "<unknown>");
    }
    (void)pthread_mutex_unlock(&trace_lock);
}

static void tracked_path(void *handle, char *path, size_t path_size)
{
    size_t index;
    const char *found = "<untracked>";

    if (path == NULL || path_size == 0U) return;
    (void)pthread_mutex_lock(&trace_lock);
    for (index = 0U; index < TRACKED_FILE_CAPACITY; ++index) {
        if (tracked_files[index].handle == handle) {
            found = tracked_files[index].path;
            break;
        }
    }
    (void)snprintf(path, path_size, "%s", found);
    (void)pthread_mutex_unlock(&trace_lock);
}

static void untrack_file(void *handle)
{
    size_t index;

    (void)pthread_mutex_lock(&trace_lock);
    for (index = 0U; index < TRACKED_FILE_CAPACITY; ++index) {
        if (tracked_files[index].handle == handle) {
            tracked_files[index].handle = NULL;
            tracked_files[index].path[0] = '\0';
            break;
        }
    }
    (void)pthread_mutex_unlock(&trace_lock);
}

static void describe_sound_source(const char *name_or_data, uint32_t mode,
                                  char *text, size_t text_size)
{
    if (text == NULL || text_size == 0U) return;
    if (name_or_data == NULL) {
        (void)snprintf(text, text_size, "<null>");
    } else if ((mode & (FMOD_OPENMEMORY_VALUE |
                        FMOD_OPENMEMORY_POINT_VALUE)) != 0U) {
        (void)snprintf(text, text_size, "<memory@%p>",
                       (const void *)name_or_data);
    } else {
        (void)snprintf(text, text_size, "%.383s", name_or_data);
    }
}

static int mp3_software_override_enabled(void)
{
    const char *configured = getenv("NFSMW_FMOD_MP3_SOFTWARE");

    return configured == NULL || strcmp(configured, "0") != 0;
}

static int mp3_stream_retry_enabled(void)
{
    const char *configured = getenv("NFSMW_FMOD_MP3_CREATESTREAM");

    return configured == NULL || strcmp(configured, "0") != 0;
}

static int mp3_memory_ab_enabled(void)
{
    const char *configured = getenv("NFSMW_FMOD_MP3_MEMORY_AB");

    return configured != NULL && strcmp(configured, "1") == 0;
}

static const char *mp3_memory_ab_target(void)
{
    const char *configured = getenv("NFSMW_FMOD_MP3_MEMORY_TARGET");

    return configured != NULL && configured[0] != '\0' ?
           configured : FMOD_MEMORY_AB_DEFAULT_PATH;
}

static int mp3_memory_ab_source_enabled(const char *source)
{
    const char *all = getenv("NFSMW_FMOD_MP3_MEMORY_ALL");

    if (all != NULL && strcmp(all, "1") == 0) {
        const char *prefix = "/published/sounds/music/";
        const size_t prefix_length = strlen(prefix);
        const size_t source_length = source != NULL ? strlen(source) : 0U;

        return source != NULL &&
               strncmp(source, prefix, prefix_length) == 0 &&
               source_length > prefix_length + 4U &&
               strcmp(source + source_length - 4U, ".mp3") == 0;
    }
    return source != NULL && strcmp(source, mp3_memory_ab_target()) == 0;
}

static int async_eof_compat_enabled(void)
{
    const char *configured = getenv("NFSMW_FMOD_ASYNC_EOF_OK");

    return configured != NULL && strcmp(configured, "1") == 0;
}

static void snapshot_file_counters(unsigned int counters[4])
{
    (void)pthread_mutex_lock(&trace_lock);
    counters[0] = file_open_calls;
    counters[1] = file_read_calls;
    counters[2] = file_async_read_calls;
    counters[3] = file_close_calls;
    (void)pthread_mutex_unlock(&trace_lock);
}

static int load_mp3_memory_ab(const char *name,
                               struct mp3_memory_ab_asset *asset)
{
    void *handle = NULL;
    void *userdata = NULL;
    void *data = NULL;
    struct fmod_44006_exinfo_arm32 *exinfo = NULL;
    uint32_t file_size = 0U;
    uint32_t total = 0U;
    fmod_result result;

    (void)printf("G8-MP3-MEMAB load-begin path=%s\n", name);
    if (original_file_open == NULL || original_file_read == NULL ||
        original_file_close == NULL) {
        (void)printf("G8-MP3-MEMAB load-refused callbacks-incomplete\n");
        return -1;
    }

    result = original_file_open(name, 0, &file_size, &handle, &userdata);
    (void)printf("G8-MP3-MEMAB load-open result=%d size=%u handle=%p\n",
                 (int)result, file_size, handle);
    if (result != 0 || handle == NULL || file_size == 0U ||
        file_size > FMOD_MEMORY_AB_MAX_SIZE ||
        (strcmp(name, FMOD_MEMORY_AB_DEFAULT_PATH) == 0 &&
         file_size != FMOD_MEMORY_AB_EXPECTED_SIZE)) {
        if (handle != NULL)
            (void)original_file_close(handle, userdata);
        return -1;
    }

    data = malloc((size_t)file_size);
    exinfo = calloc(1U, sizeof(*exinfo));
    if (data == NULL || exinfo == NULL) {
        free(data);
        free(exinfo);
        (void)original_file_close(handle, userdata);
        (void)printf("G8-MP3-MEMAB load-refused allocation-failed\n");
        return -1;
    }

    while (total < file_size) {
        const uint32_t remaining = file_size - total;
        const unsigned int requested =
            remaining < FMOD_MEMORY_READ_CHUNK ?
            (unsigned int)remaining : FMOD_MEMORY_READ_CHUNK;
        unsigned int received = 0U;

        result = original_file_read(handle,
                                    (unsigned char *)data + total,
                                    requested, &received, userdata);
        if (received > requested) {
            (void)printf("G8-MP3-MEMAB load-read-invalid offset=%u "
                         "requested=%u received=%u result=%d\n",
                         total, requested, received, (int)result);
            free(data);
            free(exinfo);
            (void)original_file_close(handle, userdata);
            return -1;
        }
        total += received;
        if (result != 0 &&
            !(result == FMOD_FILE_EOF_VALUE && total == file_size)) {
            (void)printf("G8-MP3-MEMAB load-read-failed total=%u "
                         "requested=%u received=%u result=%d\n",
                         total, requested, received, (int)result);
            free(data);
            free(exinfo);
            (void)original_file_close(handle, userdata);
            return -1;
        }
        if (received == 0U && total < file_size) {
            (void)printf("G8-MP3-MEMAB load-read-short total=%u size=%u\n",
                         total, file_size);
            free(data);
            free(exinfo);
            (void)original_file_close(handle, userdata);
            return -1;
        }
    }

    (void)printf("G8-MP3-MEMAB load-complete bytes=%u\n", total);
    result = original_file_close(handle, userdata);
    (void)printf("G8-MP3-MEMAB load-close result=%d\n", (int)result);
    if (result != 0) {
        free(data);
        free(exinfo);
        return -1;
    }

    exinfo->cbsize = (int32_t)FMOD_44006_EXINFO_SIZE;
    exinfo->length = file_size;
    (void)snprintf(asset->path, sizeof(asset->path), "%s", name);
    asset->data = data;
    asset->exinfo = exinfo;
    asset->length = file_size;
    asset->loaded = 1;
    asset->loading = 0;
    (void)printf("G8-MP3-MEMAB exinfo address=%p cbsize=%d length=%u\n",
                 (void *)exinfo, (int)exinfo->cbsize, exinfo->length);
    return 0;
}

static fmod_result try_mp3_memory_ab(
    void *system, const char *name_or_data, uint32_t mode,
    void *extra_information, void **sound, int *handled)
{
    unsigned int before[4];
    unsigned int after[4];
    struct mp3_memory_ab_asset *asset = NULL;
    int needs_load = 0;
    fmod_result result;
    unsigned int index;

    *handled = 0;
    if (mode != 0x000000a0U || extra_information != NULL || sound == NULL) {
        (void)printf("G8-MP3-MEMAB refused mode=0x%08x exinfo=%p sound-out=%p\n",
                     mode, extra_information, (void *)sound);
        return 0;
    }

    (void)pthread_mutex_lock(&trace_lock);
    for (index = 0U; index < memory_ab_asset_count; ++index) {
        if (strcmp(memory_ab_assets[index].path, name_or_data) == 0) {
            asset = &memory_ab_assets[index];
            break;
        }
    }
    if (asset != NULL && asset->failed != 0) {
        (void)pthread_mutex_unlock(&trace_lock);
        (void)printf("G8-MP3-MEMAB already-failed path=%s fallback=normal\n",
                     name_or_data);
        return 0;
    }
    if (asset != NULL && asset->loading != 0) {
        (void)pthread_mutex_unlock(&trace_lock);
        (void)printf("G8-MP3-MEMAB loading path=%s fallback=normal\n",
                     name_or_data);
        return 0;
    }
    if (asset == NULL) {
        if (memory_ab_asset_count >= FMOD_MEMORY_AB_ASSET_CAPACITY) {
            (void)pthread_mutex_unlock(&trace_lock);
            (void)printf("G8-MP3-MEMAB capacity=%u path=%s fallback=normal\n",
                         FMOD_MEMORY_AB_ASSET_CAPACITY, name_or_data);
            return 0;
        }
        asset = &memory_ab_assets[memory_ab_asset_count++];
        (void)memset(asset, 0, sizeof(*asset));
        (void)snprintf(asset->path, sizeof(asset->path), "%s", name_or_data);
        asset->loading = 1;
        asset->attempted = 1;
        needs_load = 1;
    }
    (void)pthread_mutex_unlock(&trace_lock);

    if (needs_load != 0 && load_mp3_memory_ab(name_or_data, asset) != 0) {
        (void)pthread_mutex_lock(&trace_lock);
        asset->loading = 0;
        asset->failed = 1;
        (void)pthread_mutex_unlock(&trace_lock);
        (void)printf("G8-MP3-MEMAB load-failed path=%s fallback=normal\n",
                     name_or_data);
        return 0;
    }

    snapshot_file_counters(before);
    (void)memset(asset->exinfo, 0, sizeof(*asset->exinfo));
    asset->exinfo->cbsize = (int32_t)FMOD_44006_EXINFO_SIZE;
    asset->exinfo->length = asset->length;
    *sound = NULL;
    (void)printf("G8-MP3-MEMAB create-begin path=%s data=%p bytes=%u "
                 "exinfo=%p cbsize=%d length=%u mode=0x%08x\n",
                 name_or_data, asset->data, asset->length,
                 (void *)asset->exinfo, (int)asset->exinfo->cbsize,
                 asset->exinfo->length, FMOD_MEMORY_MODE);
    result = original_create_sound(system, (const char *)asset->data,
                                   FMOD_MEMORY_MODE, asset->exinfo, sound);
    snapshot_file_counters(after);
    (void)printf("G8-MP3-MEMAB create-end result=%d sound=%p "
                 "fs-open=%u->%u fs-read=%u->%u "
                 "fs-async=%u->%u fs-close=%u->%u\n",
                 (int)result, *sound,
                 before[0], after[0], before[1], after[1],
                 before[2], after[2], before[3], after[3]);
    if (result != 0) {
        (void)pthread_mutex_lock(&trace_lock);
        asset->failed = 1;
        (void)pthread_mutex_unlock(&trace_lock);
    }
    *handled = 1;
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_file_open(
    const char *name, int unicode, uint32_t *filesize,
    void **handle, void **userdata)
{
    unsigned int call = next_counter(&file_open_calls);
    fmod_result result;
    const char *path = unicode == 0 && name != NULL ? name : "<unicode-or-null>";
    void *opened_handle;
    uint32_t opened_size;

    result = original_file_open(name, unicode, filesize, handle, userdata);
    opened_handle = handle != NULL ? *handle : NULL;
    opened_size = filesize != NULL ? *filesize : 0U;
    if (result == 0 && opened_handle != NULL) track_file(opened_handle, path);
    if (sampled(call) != 0 || result != 0) {
        (void)printf("G8-FS open call=%u path=%.383s unicode=%d "
                     "result=%d size=%u handle=%p userdata=%p\n",
                     call, path, unicode, (int)result, opened_size,
                     opened_handle, userdata != NULL ? *userdata : NULL);
    }
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_file_close(void *handle,
                                                     void *userdata)
{
    char path[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&file_close_calls);
    fmod_result result;

    tracked_path(handle, path, sizeof(path));
    result = original_file_close(handle, userdata);
    if (sampled(call) != 0 || result != 0) {
        (void)printf("G8-FS close call=%u path=%s handle=%p result=%d\n",
                     call, path, handle, (int)result);
    }
    untrack_file(handle);
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_file_read(
    void *handle, void *buffer, unsigned int size,
    unsigned int *read_size, void *userdata)
{
    char path[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&file_read_calls);
    fmod_result result;
    unsigned int actual = read_size != NULL ? *read_size : 0U;

    tracked_path(handle, path, sizeof(path));
    result = original_file_read(handle, buffer, size, read_size, userdata);
    actual = read_size != NULL ? *read_size : actual;
    if (sampled(call) != 0 || result != 0) {
        (void)printf("G8-FS read call=%u path=%s handle=%p want=%u got=%u "
                     "result=%d\n", call, path, handle, size, actual,
                     (int)result);
    }
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_file_seek(void *handle,
                                                    uint32_t position,
                                                    void *userdata)
{
    char path[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&file_seek_calls);
    fmod_result result;

    tracked_path(handle, path, sizeof(path));
    result = original_file_seek(handle, position, userdata);
    if (sampled(call) != 0 || result != 0) {
        (void)printf("G8-FS seek call=%u path=%s handle=%p position=%u "
                     "result=%d\n",
                     call, path, handle, position, (int)result);
    }
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_file_async_read(
    void *information, void *userdata)
{
    const struct fmod_async_read_info_44006 *info = information;
    char path[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&file_async_read_calls);
    fmod_result callback_result;
    void *handle = info != NULL ? info->handle : NULL;
    uint32_t offset = info != NULL ? info->offset : 0U;
    uint32_t size_bytes = info != NULL ? info->size_bytes : 0U;
    int32_t priority = info != NULL ? info->priority : 0;

    tracked_path(handle, path, sizeof(path));
    if (sampled(call) != 0) {
        (void)printf("G8-FS async-read before call=%u path=%s handle=%p "
                     "offset=%u bytes=%u priority=%d info=%p buffer=%p "
                     "bytesread=%u completion-result=%d userdata=%p\n",
                     call, path, handle, offset, size_bytes,
                     (int)priority, information,
                     info != NULL ? info->buffer : NULL,
                     info != NULL ? info->bytesread : 0U,
                     info != NULL ? (int)info->completion_result : 0,
                     info != NULL ? info->userdata : NULL);
    }
    callback_result = original_file_async_read(information, userdata);
    if (callback_result == FMOD_FILE_EOF_VALUE && info != NULL &&
        info->completion_result == FMOD_FILE_EOF_VALUE &&
        async_eof_compat_enabled() != 0) {
        callback_result = 0;
        (void)printf("G8-FS async-read eof-normalized call=%u path=%s "
                     "bytesread=%u completion-result=%d\n",
                     call, path, info->bytesread,
                     (int)info->completion_result);
    }
    if (sampled(call) != 0 || callback_result != 0) {
        (void)printf("G8-FS async-read after call=%u path=%s "
                     "callback-result=%d buffer=%p bytesread=%u "
                     "completion-result=%d userdata=%p\n",
                     call, path, (int)callback_result,
                     info != NULL ? info->buffer : NULL,
                     info != NULL ? info->bytesread : 0U,
                     info != NULL ? (int)info->completion_result : 0,
                     info != NULL ? info->userdata : NULL);
    }
    return callback_result;
}

static fmod_result NFSMW_GUEST_ABI trace_file_async_cancel(
    void *handle, void *userdata)
{
    char path[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&file_async_cancel_calls);
    fmod_result result;

    tracked_path(handle, path, sizeof(path));
    result = original_file_async_cancel(handle, userdata);
    if (sampled(call) != 0 || result != 0) {
        (void)printf("G8-FS async-cancel call=%u path=%s handle=%p result=%d\n",
                     call, path, handle, (int)result);
    }
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_create_sound(
    void *system, const char *name_or_data, uint32_t mode,
    void *extra_information, void **sound)
{
    char source[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&create_sound_calls);
    fmod_result result;
    void *created;
    uint32_t effective_mode = mode;
    int mp3_software_override = 0;
    int stream_retry = 0;
    int stream_result = 0;
    int memory_ab_handled = 0;

    if (version_logged == 0 && original_get_version != NULL) {
        uint32_t version = 0U;
        fmod_result version_result = original_get_version(system, &version);

        (void)printf("G8-FMOD version-result=%d version=0x%08x\n",
                     (int)version_result, (unsigned int)version);
        version_logged = 1;
    }

    describe_sound_source(name_or_data, mode, source, sizeof(source));
    if (mp3_memory_ab_enabled() != 0 &&
        mp3_memory_ab_source_enabled(source) != 0) {
        result = try_mp3_memory_ab(system, name_or_data, mode,
                                   extra_information, sound,
                                   &memory_ab_handled);
        if (memory_ab_handled != 0) return result;
    }
    if (strstr(source, ".mp3") != NULL &&
        (mode & FMOD_HARDWARE_VALUE) != 0U &&
        mp3_software_override_enabled() != 0) {
        effective_mode = (mode & ~(uint32_t)FMOD_HARDWARE_VALUE) |
                         (uint32_t)FMOD_SOFTWARE_VALUE;
        mp3_software_override = 1;
    }
    result = original_create_sound(system, name_or_data, effective_mode,
                                   extra_information, sound);
    if (result != 0 && strstr(source, ".mp3") != NULL &&
        mp3_stream_retry_enabled() != 0 && original_create_stream != NULL) {
        stream_retry = 1;
        stream_result = original_create_stream(system, name_or_data,
                                                effective_mode,
                                                extra_information, sound);
        if (stream_result == 0) result = stream_result;
    }
    created = sound != NULL ? *sound : NULL;
    if (sampled(call) != 0 ||
        (strstr(source, ".mp3") != NULL && call % 128U == 0U)) {
        (void)printf("G8-CREATE-SOUND call=%u source=%s mode=0x%08x "
                     "effective=0x%08x mp3-software=%d exinfo=%p "
                     "stream-retry=%d stream-result=%d result=%d sound=%p\n",
                     call, source, mode, effective_mode,
                     mp3_software_override, extra_information,
                     stream_retry, stream_result, (int)result, created);
    }
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_create_stream(
    void *system, const char *name_or_data, uint32_t mode,
    void *extra_information, void **sound)
{
    char source[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&create_stream_calls);
    fmod_result result;
    void *created;

    describe_sound_source(name_or_data, mode, source, sizeof(source));
    result = original_create_stream(system, name_or_data, mode,
                                    extra_information, sound);
    created = sound != NULL ? *sound : NULL;
    if (sampled(call) != 0 ||
        (strstr(source, ".mp3") != NULL && call % 128U == 0U)) {
        (void)printf("G8-CREATE-STREAM call=%u source=%s mode=0x%08x "
                     "exinfo=%p result=%d sound=%p\n",
                     call, source, mode, extra_information,
                     (int)result, created);
    }
    return result;
}

static fmod_result NFSMW_GUEST_ABI trace_set_file_system(
    void *system,
    fmod_file_open_callback user_open,
    fmod_file_close_callback user_close,
    fmod_file_read_callback user_read,
    fmod_file_seek_callback user_seek,
    fmod_file_async_read_callback user_async_read,
    fmod_file_async_cancel_callback user_async_cancel,
    int block_align)
{
    fmod_result result;

    original_file_open = user_open;
    original_file_close = user_close;
    original_file_read = user_read;
    original_file_seek = user_seek;
    original_file_async_read = user_async_read;
    original_file_async_cancel = user_async_cancel;
    (void)printf("G8-FS setFileSystem open=%d close=%d read=%d seek=%d "
                 "async-read=%d async-cancel=%d block-align=%d\n",
                 user_open != NULL, user_close != NULL, user_read != NULL,
                 user_seek != NULL,
                 user_async_read != NULL, user_async_cancel != NULL,
                 block_align);
    result = original_set_file_system(
        system,
        user_open != NULL ? trace_file_open : NULL,
        user_close != NULL ? trace_file_close : NULL,
        user_read != NULL ? trace_file_read : NULL,
        user_seek != NULL ? trace_file_seek : NULL,
        user_async_read != NULL ? trace_file_async_read : NULL,
        user_async_cancel != NULL ? trace_file_async_cancel : NULL,
        block_align);
    (void)printf("G8-FS setFileSystem result=%d\n", (int)result);
    return result;
}

static int bind_function(void *destination, size_t destination_size,
                         uintptr_t address, const char *label,
                         char *error, size_t error_size)
{
    if (address == 0U || destination_size != sizeof(address)) {
        (void)snprintf(error, error_size,
                       "cannot bind FMOD trace target %s", label);
        return -1;
    }
    (void)memcpy(destination, &address, sizeof(address));
    return 0;
}

int nfsmw_fmod_trace_bind(const struct elf32_image *fmod_image,
                          char *error, size_t error_size)
{
    const char *configured = getenv("NFSMW_FMOD_TRACE");
    uintptr_t create_sound_address;
    uintptr_t create_stream_address;
    uintptr_t get_version_address;
    uintptr_t set_file_system_address;

    if (configured != NULL && strcmp(configured, "0") == 0) {
        trace_enabled = 0;
        (void)printf("G8-FMODTRACE disabled by NFSMW_FMOD_TRACE=0\n");
        return 0;
    }
    if (fmod_image == NULL || fmod_image->soname == NULL ||
        strcmp(fmod_image->soname, "libfmodex.so") != 0) {
        (void)snprintf(error, error_size,
                       "FMOD trace did not receive libfmodex.so");
        return -1;
    }
    create_sound_address = elf32_find_export(fmod_image, create_sound_symbol);
    create_stream_address = elf32_find_export(fmod_image, create_stream_symbol);
    get_version_address = elf32_find_export(fmod_image, get_version_symbol);
    set_file_system_address = elf32_find_export(fmod_image,
                                                set_file_system_symbol);
    if (bind_function(&original_create_sound, sizeof(original_create_sound),
                      create_sound_address, "System::createSound",
                      error, error_size) != 0 ||
        bind_function(&original_create_stream, sizeof(original_create_stream),
                      create_stream_address, "System::createStream",
                      error, error_size) != 0 ||
        bind_function(&original_get_version, sizeof(original_get_version),
                      get_version_address, "System::getVersion",
                      error, error_size) != 0 ||
        bind_function(&original_set_file_system,
                      sizeof(original_set_file_system),
                      set_file_system_address, "System::setFileSystem",
                      error, error_size) != 0) {
        return -1;
    }
    trace_enabled = 1;
    version_logged = 0;
    (void)printf("G8-FMODTRACE bound createSound/createStream/getVersion/"
                 "setFileSystem\n");
    return 0;
}

static uintptr_t create_sound_hook_address(void)
{
    fmod_create_sound_function function = trace_create_sound;
    uintptr_t address = 0U;

    if (sizeof(function) == sizeof(address))
        (void)memcpy(&address, &function, sizeof(address));
    return address;
}

static uintptr_t create_stream_hook_address(void)
{
    fmod_create_sound_function function = trace_create_stream;
    uintptr_t address = 0U;

    if (sizeof(function) == sizeof(address))
        (void)memcpy(&address, &function, sizeof(address));
    return address;
}

static uintptr_t set_file_system_hook_address(void)
{
    fmod_set_file_system_function function = trace_set_file_system;
    uintptr_t address = 0U;

    if (sizeof(function) == sizeof(address))
        (void)memcpy(&address, &function, sizeof(address));
    return address;
}

uintptr_t nfsmw_fmod_trace_resolve(const char *requesting_soname,
                                   const char *name)
{
    if (trace_enabled == 0 || requesting_soname == NULL || name == NULL)
        return 0U;
    if (strcmp(requesting_soname, "libapp.so") != 0 &&
        strcmp(requesting_soname, "libfmodevent.so") != 0)
        return 0U;
    if (strcmp(name, create_sound_symbol) == 0)
        return create_sound_hook_address();
    if (strcmp(name, create_stream_symbol) == 0)
        return create_stream_hook_address();
    if (strcmp(name, set_file_system_symbol) == 0)
        return set_file_system_hook_address();
    return 0U;
}
