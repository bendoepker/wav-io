
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//NOTE:
//      1:  Some wav files have a pattern on the end that begins with "acid"
//          this data is big endian encoded and is proprietary to Magix Acid,
//          however it contains potentially useful data i.e. tempo, meter (4/4),
//          looping, stretch, disk/ram based, etc.. It may be useful for use in
//          ByteWave in the future
//      2:  Zig
//          Pros
//              Error Handling
//              Readability
//              Better Testing (zig test ...)
//              Cross platform
//          Cons
//              Unstable
//              Misleading Syntax (Error unions and slices mainly)
//              Must be coerced to the C ABI (export c...)
//      3:  C
//          Pros
//              Stable
//              Easily integratable within ByteWave
//          Cons
//              Error prone
//              Needs to be macroed to hell for cross platform ability
//      4: The wav_read and wav_write function can be simplified by 

static const char* CHUNK_ID_C = "RIFF";
static const char* FORMAT_C = "WAVE";
static const char* SUB_CHUNK_1_ID_C = "fmt ";
static const char* SUB_CHUNK_2_ID_C = "data";

typedef enum error_code {
    E_OK = 0,           //0
    E_FILE_NOT_OPENED,  //1
    E_FILE_READ_FAIL,   //2
    E_FILE_WRITE_FAIL,  //3
    E_FILE_POS_FAIL,    //4
    E_MALFORMED_HEADER, //5
    E_NON_PCM_DATA,     //6
    E_FAILED_MALLOC,    //7
} error_code;

typedef struct wav_metadata {
    FILE* wav_fp;

    int16_t audio_format;       //This should be 1 for PCM (Integer) or 3 for IEEE754 (floating point)
    int16_t num_channels;
    int32_t sample_rate;


    //Remove the next two since they are calculatable
    int32_t byte_rate;          //This can be calculated as sample_rate * num_channels * bits_per_sample/8
    int16_t block_align;        //This can be calculated as num_channels * bits_per_sample/8 

    int16_t bits_per_sample;    //Number of Bits in each sample (f32 -> 32)

    uint64_t data_pos;          //This is the location (in bytes) of the first sample in the file
    uint64_t num_samples;       //This is the number of samples in the data block (for stereo audio both
                                //channels are contained in the same "sample")
} wav_metadata;

typedef enum tagged_value {
    string = 0,
    integer = 1,
} tagged_value;

typedef struct value_t {
    tagged_value value_type;
    int value_s_len;
    const char* value_s;
    int32_t value_i;
} value_t;

//Allocation of data on the heap and containing file pointer
//NOTE: These files are not thread safe (malloc / free usage)
error_code create_wav_metadata(FILE* file, wav_metadata** metadata);
void destroy_wav_metadata(wav_metadata* metadata);

//Read and write the contents of @param metadata to and from a file
error_code read_wav_metadata(wav_metadata* metadata);
error_code write_wav_metadata(wav_metadata* metadata);

error_code open_wav_read(char* file_name,
                         int16_t* audio_format,
                         int16_t* num_channels,
                         int32_t* sample_rate,
                         int16_t* bits_per_sample,
                         int64_t* data_pos,
                         int64_t* num_samples);

error_code open_wav_write(int16_t audio_format,
                          int16_t num_channels,
                          int32_t sample_rate,
                          int16_t bits_per_sample,
                          int64_t num_sam);

//Read and write data to a wav file
error_code read_wav_data(wav_metadata* metadata, void* data, size_t data_len);
error_code write_wav_data(wav_metadata* metadata, void* data, size_t data_len);

//Print the metadata info to stdout
void print_metadata(wav_metadata* metadata);
