#ifndef INCLUDED_COLLADA_DLL
#define INCLUDED_COLLADA_DLL

#ifdef _WIN32
# ifdef COLLADA_DLL
#  define EXPORT extern "C" __declspec(dllexport)
# else
#  define EXPORT extern "C" __declspec(dllimport)
# endif
#else
# if __GNUC__ >= 4
#  define EXPORT extern "C" __attribute__ ((visibility ("default")))
# else
#  define EXPORT extern "C"
# endif
#endif

#define LOG_INFO 0
#define LOG_WARNING 1
#define LOG_ERROR 2

typedef void (*LogFn) (int severity, const char* text);
typedef void (*OutputFn) (void* cb_data, const char* data, unsigned int length);

#define COLLADA_CONVERTER_VERSION 1

EXPORT void set_logger(LogFn logger);
EXPORT int set_skeleton_definitions(const char* xml, int length);
EXPORT int convert_dae_to_pmd(const char* dae, OutputFn pmd_writer, void* cb_data);
EXPORT int convert_dae_to_psa(const char* dae, OutputFn psa_writer, void* cb_data);

#endif /* INCLUDED_COLLADA_DLL */
