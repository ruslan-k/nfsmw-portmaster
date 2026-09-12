#include "fmod_trace.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if UINTPTR_MAX != UINT32_MAX
#error "The NFS MW FMOD trace bridge requires the 32-bit ARM guest runtime"
#endif

#define NFSMW_GUEST_ABI __attribute__((pcs("aapcs")))

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
typedef fmod_result (NFSMW_GUEST_ABI *fmod_set_file_system_function)(
    void *system,
    fmod_file_open_callback user_open,
    fmod_file_close_callback user_close,
    fmod_file_read_callback user_read,
    fmod_file_seek_callback user_seek,
    fmod_file_async_read_callback user_async_read,
    fmod_file_async_cancel_callback user_async_cancel,
    int block_align);

/*
 * This is the stable prefix of FMOD_ASYNCREADINFO used by the bundled FMOD
 * generation.  The trace bridge only reads this prefix and never mutates it.
 */
struct fmod_async_read_prefix {
    void *handle;
    uint32_t offset;
    uint32_t size_bytes;
    int32_t priority;
};

enum {
    TRACKED_FILE_CAPACITY = 64,
    TRACKED_PATH_CAPACITY = 384,
    FMOD_HARDWARE_VALUE = 0x00000020U,
    FMOD_SOFTWARE_VALUE = 0x00000040U,
    FMOD_OPENMEMORY_VALUE = 0x00000800U,
    FMOD_OPENMEMORY_POINT_VALUE = 0x10000000U
};

struct tracked_file {
    void *handle;
    char path[TRACKED_PATH_CAPACITY];
};

static const char create_sound_symbol[] =
    "_ZN4FMOD6System11createSoundEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE";
static const char create_stream_symbol[] =
    "_ZN4FMOD6System12createStreamEPKcjP22FMOD_CREATESOUNDEXINFOPPNS_5SoundE";
static const char set_file_system_symbol[] =
    "_ZN4FMOD6System13setFileSystemEPF11FMOD_RESULTPKciPjPPvS6_"
    "EPFS1_S5_S5_EPFS1_S5_S5_jS4_S5_EPFS1_S5_jS5_"
    "EPFS1_P18FMOD_ASYNCREADINFOS5_ESA_i";

static fmod_create_sound_function original_create_sound;
static fmod_create_sound_function original_create_stream;
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
    const struct fmod_async_read_prefix *info = information;
    char path[TRACKED_PATH_CAPACITY];
    unsigned int call = next_counter(&file_async_read_calls);
    fmod_result result;
    void *handle = info != NULL ? info->handle : NULL;
    uint32_t offset = info != NULL ? info->offset : 0U;
    uint32_t size_bytes = info != NULL ? info->size_bytes : 0U;
    int32_t priority = info != NULL ? info->priority : 0;

    tracked_path(handle, path, sizeof(path));
    if (sampled(call) != 0) {
        (void)printf("G8-FS async-read begin call=%u path=%s handle=%p "
                     "offset=%u bytes=%u priority=%d info=%p\n",
                     call, path, handle, offset, size_bytes,
                     (int)priority, information);
    }
    result = original_file_async_read(information, userdata);
    if (sampled(call) != 0 || result != 0) {
        (void)printf("G8-FS async-read return call=%u path=%s result=%d\n",
                     call, path, (int)result);
    }
    return result;
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

    describe_sound_source(name_or_data, mode, source, sizeof(source));
    if (strstr(source, ".mp3") != NULL &&
        (mode & FMOD_HARDWARE_VALUE) != 0U &&
        mp3_software_override_enabled() != 0) {
        effective_mode = (mode & ~FMOD_HARDWARE_VALUE) |
                         FMOD_SOFTWARE_VALUE;
        mp3_software_override = 1;
    }
    result = original_create_sound(system, name_or_data, effective_mode,
                                   extra_information, sound);
    created = sound != NULL ? *sound : NULL;
    if (sampled(call) != 0 ||
        (strstr(source, ".mp3") != NULL && call % 128U == 0U)) {
        (void)printf("G8-CREATE-SOUND call=%u source=%s mode=0x%08x "
                     "effective=0x%08x mp3-software=%d exinfo=%p "
                     "result=%d sound=%p\n",
                     call, source, mode, effective_mode,
                     mp3_software_override, extra_information,
                     (int)result, created);
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
    set_file_system_address = elf32_find_export(fmod_image,
                                                set_file_system_symbol);
    if (bind_function(&original_create_sound, sizeof(original_create_sound),
                      create_sound_address, "System::createSound",
                      error, error_size) != 0 ||
        bind_function(&original_create_stream, sizeof(original_create_stream),
                      create_stream_address, "System::createStream",
                      error, error_size) != 0 ||
        bind_function(&original_set_file_system,
                      sizeof(original_set_file_system),
                      set_file_system_address, "System::setFileSystem",
                      error, error_size) != 0) {
        return -1;
    }
    trace_enabled = 1;
    (void)printf("G8-FMODTRACE bound createSound/createStream/setFileSystem\n");
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
