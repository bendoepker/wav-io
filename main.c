#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char* CHUNKID_C = "RIFF";
static const char* FORMAT_C = "WAVE";
static const char* SUBCHUNK1ID_C = "fmt ";
static const char* SUBCHUNK2ID_C = "data";

typedef enum error_code {
    E_OK = 0,           //0
    E_FILE_NOT_OPENED,  //1
    E_FILE_READ_FAIL,   //2
    E_MALFORMED_HEADER, //3
    E_NON_PCM_DATA,     //4
    E_FAILED_MALLOC,    //5
} error_code;

typedef struct wav_metadata {
    int32_t ChunkSize;
    int32_t SubChunk1Size;
    int16_t AudioFormat;        //This should be 1 for PCM (Integer) or 3 for IEEE754 (floating point)
    int16_t NumChannels;
    int32_t SampleRate;
    int32_t ByteRate;           //This can be calculated as SampleRate * NumChannels * BitsPerSample/8
    int16_t BlockAlign;         //This can be calculated as NumChannels * BitsPerSample/8 
                                //(Number of Bytes for one sample from all channels)
    int16_t BitsPerSample;

    int16_t ExtraParamSize;     //These ExtraParams are for Non PCM or IEEE754 data (probably will not be used)
    uint8_t* ExtraParams;       //This needs to be malloced and freed

    int32_t SubChunk2Size;      //This is equal to NumSamples * NumChannels * BitsPerSample/8 
                                //(and should be set to 0 until the file is done being written, then calculated)
    uint64_t DataPos;           //This is a pointer to the position of the beginning of the data block of the file
} wav_metadata;

typedef enum tagged_value {
    string = 0,
    integer = 1,
} tagged_value;

typedef struct value_t {
    tagged_value value_type;
    int value_s_len;
    char* value_s;
    int32_t value_i;
} value_t;

error_code read_wav(FILE* input_file, wav_metadata** metadata);
void formatted_print_header();
void formatted_print(int64_t offset,
                     size_t bytes,
                     char* identifier, int identifier_len,
                     value_t value);

//NOTE:
//      1:  Some wav files have a pattern on the end that begins with "acid"
//          this data is big endian encoded and is proprietary to Magix Acid,
//          however it contains potentially useful data i.e. tempo, meter (4/4),
//          looping, stretch, disk/ram based, etc.. It may be useful for use in
//          ByteWave

int main() {
    FILE* fp = 0;
    fp = fopen("test.wav", "r");
    if(fp == 0) {
        printf("Failed to open file\n");
        return 1;
    }

    wav_metadata* md;
    error_code res = read_wav(fp, &md);
    if(res != E_OK) {
        printf("read_wav failed with error code %d", res);
        return 1;
    }

    free(md);
    fclose(fp);
}

//TODO: error handling for fgetpos()
error_code read_wav(FILE* input_file, wav_metadata** metadata) {
    error_code out = E_OK;
    *metadata = (wav_metadata*)malloc(sizeof(wav_metadata));
    int64_t offset = 0;
    uint64_t bytes = 0;

    //Header ID's
    char* ChunkID = (char*)malloc(4);
    char* Format = (char*)malloc(4);
    char* SubChunk1ID = (char*)malloc(4);
    char* SubChunk2ID = (char*)malloc(4);
    if(ChunkID == 0
        || Format == 0
        || SubChunk1ID == 0
        || SubChunk2ID == 0) return E_FAILED_MALLOC;

    if(input_file == 0) return E_FILE_NOT_OPENED;

    //Print the Format Header
    formatted_print_header();

    //Chunk ID
    fgetpos(input_file, &offset);
    bytes = fread(ChunkID, sizeof(typeof(*ChunkID)), 4, input_file);
    if(bytes != sizeof(char) * 4) return E_FILE_READ_FAIL;
    if(strncmp(ChunkID, CHUNKID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed
    formatted_print(offset, bytes, "Chunk ID", 8, (value_t){string, 4, ChunkID, 0});

    //Chunk Size
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->ChunkSize, sizeof(typeof((*metadata)->ChunkSize)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->ChunkSize)); //For formatted print
    formatted_print(offset, bytes, "Chunk Size (B)", 14, (value_t){integer, 0, 0, (*metadata)->ChunkSize});
    formatted_print(INT64_MAX, SIZE_MAX, "File Size (B)", 14, (value_t){integer, 0, 0, ((*metadata)->ChunkSize + 8)});

    //Format
    fgetpos(input_file, &offset);
    bytes = fread(Format, sizeof(typeof(*Format)), 4, input_file);
    if(bytes != (sizeof(char) * 4)) return E_FILE_READ_FAIL;
    if(strncmp(Format, FORMAT_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed
    formatted_print(offset, bytes, "Format", 6, (value_t){string, 4, Format, 0});

    //Sub Chunk 1 ID
    fgetpos(input_file, &offset);
    bytes = fread(SubChunk1ID, sizeof(typeof(*SubChunk1ID)), 4, input_file);
    if(bytes != 4) return E_FILE_READ_FAIL;
    if(strncmp(SubChunk1ID, SUBCHUNK1ID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed
    formatted_print(offset, bytes, "Sub Chunk 1 ID", 14, (value_t){string, 4, SubChunk1ID, 0});

    //Sub Chunk 1 Size
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->SubChunk1Size, sizeof(typeof((*metadata)->SubChunk1Size)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->SubChunk1Size)); //For formatted print
    formatted_print(offset, bytes, "Sub Chunk 1 Size (B)", 20, (value_t){integer, 0, 0, (*metadata)->SubChunk1Size});

    //Audio Format
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->AudioFormat, sizeof(typeof((*metadata)->AudioFormat)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->AudioFormat)); //For formatted print
    formatted_print(offset, bytes, "Audio Format (1 = Int, 3 = F32)", 31, (value_t){integer, 0, 0, (*metadata)->AudioFormat});

    //Num Channels
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->NumChannels, sizeof(typeof((*metadata)->NumChannels)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->NumChannels)); //For formatted print
    formatted_print(offset, bytes, "Number of Channels", 18, (value_t){integer, 0, 0, (*metadata)->NumChannels});

    //Sample Rate
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->SampleRate, sizeof(typeof((*metadata)->SampleRate)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->SampleRate)); //For formatted print
    formatted_print(offset, bytes, "Sample Rate (Hz)", 16, (value_t){integer, 0, 0, (*metadata)->SampleRate});

    //ByteRate
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->ByteRate, sizeof(typeof((*metadata)->ByteRate)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->ByteRate)); //For formatted print
    formatted_print(offset, bytes, "Byte Rate (Hz)", 14, (value_t){integer, 0, 0, (*metadata)->ByteRate});

    //Block Align
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->BlockAlign, sizeof(typeof((*metadata)->BlockAlign)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->BlockAlign)); //For formatted print
    formatted_print(offset, bytes, "Block Align (B)", 15, (value_t){integer, 0, 0, (*metadata)->BlockAlign});

    //Bits Per Sample
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->BitsPerSample, sizeof(typeof((*metadata)->BitsPerSample)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->BitsPerSample)); //For formatted print
    formatted_print(offset, bytes, "Bits Per Sample (b)", 19, (value_t){integer, 0, 0, (*metadata)->BitsPerSample});

    //NOTE: This could be a topic for further research...
    if((*metadata)->SubChunk1Size != 16)
        return E_NON_PCM_DATA;

    //Sub Chunk 2 ID
    fgetpos(input_file, &offset);
    bytes = fread(SubChunk2ID, sizeof(typeof(*SubChunk2ID)), 4, input_file);
    if(bytes != (sizeof(char) * 4)) return E_FILE_READ_FAIL;
    if(strncmp(SubChunk2ID, SUBCHUNK2ID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed
    formatted_print(offset, bytes, "Sub Chunk 2 ID", 14, (value_t){string, 4, SubChunk2ID, 0});

    //Sub Chunk 2 Size
    fgetpos(input_file, &offset);
    bytes = fread(&(*metadata)->SubChunk2Size, sizeof(typeof((*metadata)->SubChunk2Size)), 1, input_file);
    if(bytes != 1) return E_FILE_READ_FAIL;
    bytes *= sizeof(typeof((*metadata)->SubChunk2Size)); //For formatted print
    formatted_print(offset, bytes, "Sub Chunk 2 Size (B)", 20, (value_t){integer, 0, 0, (*metadata)->SubChunk2Size});

    //Free temp data
    free(ChunkID);
    free(Format);
    free(SubChunk1ID);
    free(SubChunk2ID);
    return out;
}

void formatted_print_header() {
    printf("%-8s | %-8s | %-32s | %-8s\n", "Offset", "Bytes", "Identifier", "Value");
    printf("-------- | -------- | -------------------------------- | -------- \n");
}

//@param offset: if it is equal to INT64_MAX it will print a blank space
//@param bytes: if it is equal to SIZE_MAX it will print a blank space
void formatted_print(int64_t offset, size_t bytes, char* identifier, int identifier_len, value_t value){
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
    printf("%-32.*s | ", identifier_len, identifier);

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
