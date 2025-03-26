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

static const char* CHUNKID_C = "RIFF";
static const char* FORMAT_C = "WAVE";
static const char* SUBCHUNK1ID_C = "fmt ";
static const char* SUBCHUNK2ID_C = "data";

typedef enum error_code {
    E_OK = 0,           //0
    E_FILE_NOT_OPENED,  //1
    E_FILE_READ_FAIL,   //2
    E_FILE_POS_FAIL,    //3
    E_MALFORMED_HEADER, //4
    E_NON_PCM_DATA,     //5
    E_FAILED_MALLOC,    //6
} error_code;

typedef struct wav_metadata {
    int32_t chunk_size;         //This is the size of the entire file in bytes -8 bytes (Chunk ID and Chunk Size)

    int16_t audio_format;       //This should be 1 for PCM (Integer) or 3 for IEEE754 (floating point)
    int16_t num_channels;
    int32_t sample_rate;
    int32_t byte_rate;          //This can be calculated as sample_rate * num_channels * bits_per_sample/8
    int16_t block_align;        //This can be calculated as num_channels * bits_per_sample/8 
                                //(Number of Bytes for one sample from all channels)
    int16_t bits_per_sample;

    //Not Currently Handling These fields
    //int16_t extra_param_size; //These ExtraParams are for Non PCM or IEEE754 data (probably will not be used)
    //uint8_t* extra_params;    //This needs to be malloced and freed

                                //(and should be set to 0 until the file is done being written, then calculated)
    uint64_t data_pos;          //This is the location (in bytes) of the first sample in the file
    uint64_t num_samples;       //This is the number of samples in the data block (for stereo audio both
                                //channels are contained in the same "sample"
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

error_code read_wav_metadata(FILE* input_file, wav_metadata** metadata);
static void formatted_print_header();
static void formatted_print(int64_t offset, size_t bytes, char* identifier, value_t value);
void print_metadata(wav_metadata* metadata);

int main() {
    FILE* fp = 0;
    fp = fopen("test.wav", "r");
    if(fp == 0) {
        printf("Failed to open file\n");
        return 1;
    }

    wav_metadata* md;
    error_code res = read_wav_metadata(fp, &md);
    if(res != E_OK) {
        printf("read_wav failed with error code %d", res);
        return 1;
    }

    print_metadata(md);

    free(md);
    fclose(fp);
}

//NOTE: This function uses malloc for @param wav_metadata** but the data needs to be freed elsewhere
error_code read_wav_metadata(FILE* input_file, wav_metadata** metadata) {
    error_code out = E_OK;

    *metadata = (wav_metadata*)malloc(sizeof(wav_metadata));
    if(*metadata == 0) return E_FAILED_MALLOC;

    //Local Variables
    int64_t offset = 0;
    uint64_t items_read = 0;

    char ChunkID[4];
    char Format[4];
    char SubChunk1ID[4];
    char SubChunk2ID[4];

    int32_t sub_chunk_1_size = 0;
    int32_t sub_chunk_2_size = 0;

    if(input_file == 0) return E_FILE_NOT_OPENED;

    //Chunk ID
    items_read = fread(ChunkID, sizeof(typeof(*ChunkID)), 4, input_file);
    if(items_read != sizeof(char) * 4) return E_FILE_READ_FAIL;
    if(strncmp(ChunkID, CHUNKID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Chunk Size
    items_read = fread(&(*metadata)->chunk_size, sizeof(typeof((*metadata)->chunk_size)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Format
    items_read = fread(Format, sizeof(typeof(*Format)), 4, input_file);
    if(items_read != (sizeof(char) * 4)) return E_FILE_READ_FAIL;
    if(strncmp(Format, FORMAT_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Sub Chunk 1 ID
    items_read = fread(SubChunk1ID, sizeof(typeof(*SubChunk1ID)), 4, input_file);
    if(items_read != 4) return E_FILE_READ_FAIL;
    if(strncmp(SubChunk1ID, SUBCHUNK1ID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Sub Chunk 1 Size
    items_read = fread(&sub_chunk_1_size, sizeof(typeof(sub_chunk_1_size)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Audio Format
    items_read = fread(&(*metadata)->audio_format, sizeof(typeof((*metadata)->audio_format)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Num Channels
    items_read = fread(&(*metadata)->num_channels, sizeof(typeof((*metadata)->num_channels)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Sample Rate
    items_read = fread(&(*metadata)->sample_rate, sizeof(typeof((*metadata)->sample_rate)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //byte_rate
    items_read = fread(&(*metadata)->byte_rate, sizeof(typeof((*metadata)->byte_rate)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Block Align
    items_read = fread(&(*metadata)->block_align, sizeof(typeof((*metadata)->block_align)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Bits Per Sample
    items_read = fread(&(*metadata)->bits_per_sample, sizeof(typeof((*metadata)->bits_per_sample)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //NOTE: This could be a topic for further research...
    //      Not today though
    if(sub_chunk_1_size != 16)
        return E_NON_PCM_DATA;

    //Sub Chunk 2 ID
    items_read = fread(SubChunk2ID, sizeof(typeof(*SubChunk2ID)), 4, input_file);
    if(items_read != (sizeof(char) * 4)) return E_FILE_READ_FAIL;
    if(strncmp(SubChunk2ID, SUBCHUNK2ID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Sub Chunk 2 Size
    items_read = fread(&sub_chunk_2_size, sizeof(typeof(sub_chunk_2_size)), 1, input_file);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Data Position (using items_read for error handling)
    items_read = fgetpos(input_file, &offset);
    if(items_read != 0) return E_FILE_POS_FAIL;
    (*metadata)->data_pos = offset;

    uint16_t bytes_per_sample = (*metadata)->bits_per_sample / 8;

    //Check Byte Rate Accuracy
    if((*metadata)->byte_rate !=
        ((*metadata)->sample_rate * (*metadata)->num_channels * bytes_per_sample))
        return E_MALFORMED_HEADER;

    //Check Block Align
    if((*metadata)->block_align !=
        (*metadata)->num_channels * bytes_per_sample)
        return E_MALFORMED_HEADER;

    //Calculate Num Samples
    (*metadata)->num_samples = sub_chunk_2_size /
        ((*metadata)->num_channels * bytes_per_sample);

    return out;
}

static void formatted_print_header() {
    printf("%-8s | %-8s | %-32s | %-8s\n", "Offset", "Bytes", "Identifier", "Value");
    printf("-------- | -------- | -------------------------------- | -------- \n");
}

//@param offset: if it is equal to INT64_MAX it will print a blank space
//@param bytes: if it is equal to SIZE_MAX it will print a blank space
static void formatted_print(int64_t offset, size_t bytes, char* identifier, value_t value){
    //Print the offset (if present)
    if(offset != INT64_MAX)
        printf("%-8lld | ", offset);
    else
        printf("         | ");

    //Print the Bytes (if present)
    if(bytes != SIZE_MAX)
        printf("%-8zd | ", bytes);
    else
        printf("         | ");

    //Print the Identifier
    printf("%-32s | ", identifier);

    //Print the Value
    switch(value.value_type) {
        case string:
            printf("\"%-4.*s\"  \n", value.value_s_len, value.value_s);
            break;
        case integer:
            printf("%-8d\n", value.value_i);
            break;
    }
}

void print_metadata(wav_metadata* metadata){
    formatted_print_header();
    formatted_print(0, 4, "Chunk ID", (value_t){string, 4, CHUNKID_C, 0});
    formatted_print(4, 4, "Chunk Size (B)", (value_t){integer, 0, 0, metadata->chunk_size});
    formatted_print(INT64_MAX, SIZE_MAX, "File Size (B)", (value_t){integer, 0, 0, (metadata->chunk_size + 8)});
    formatted_print(8, 4, "Format", (value_t){string, 4, FORMAT_C, 0});
    formatted_print(12, 4, "Sub Chunk 1 ID", (value_t){string, 4, SUBCHUNK1ID_C, 0});
    formatted_print(20, 2, "Audio Format (1 = Int, 3 = F32)", (value_t){integer, 0, 0, metadata->audio_format});
    formatted_print(22, 2, "Number of Channels", (value_t){integer, 0, 0, metadata->num_channels});
    formatted_print(24, 4, "Sample Rate (Hz)", (value_t){integer, 0, 0, metadata->sample_rate});
    formatted_print(28, 4, "Byte Rate (Hz)", (value_t){integer, 0, 0, metadata->byte_rate});
    formatted_print(32, 2, "Block Align (B)", (value_t){integer, 0, 0, metadata->block_align});
    formatted_print(34, 2, "Bits Per Sample (b)", (value_t){integer, 0, 0, metadata->bits_per_sample});
    formatted_print(36, 4, "Sub Chunk 2 ID", (value_t){string, 4, SUBCHUNK2ID_C, 0});
    formatted_print(INT64_MAX, SIZE_MAX, "Number of Samples", (value_t){integer, 0, 0, metadata->num_samples});
    formatted_print(INT64_MAX, SIZE_MAX, "Data Chunk Position", (value_t){integer, 0, 0, metadata->data_pos});
}
