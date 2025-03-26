#include "wav.h"

error_code create_wav_metadata(FILE* file, wav_metadata** metadata){
    *metadata = (wav_metadata*)malloc(sizeof(wav_metadata));
    if(*metadata == 0) return E_FAILED_MALLOC;
    else {
        memset(*metadata, 0, sizeof(wav_metadata));
        (*metadata)->wav_fp = file;
        return E_OK;
    }
}

void destroy_wav_metadata(wav_metadata* metadata){
    free(metadata);
    metadata = 0;
}

error_code read_wav_metadata(wav_metadata* metadata) {
    error_code out = E_OK;

    //Local Variables
    int64_t offset = 0;
    uint64_t items_read = 0;

    char ChunkID[4];
    char Format[4];
    char SubChunk1ID[4];
    char SubChunk2ID[4];

    int32_t chunk_size = 0;
    int32_t sub_chunk_1_size = 0;
    int32_t sub_chunk_2_size = 0;

    if(metadata->wav_fp == 0) return E_FILE_NOT_OPENED;

    //Chunk ID
    items_read = fread(ChunkID, sizeof(typeof(*ChunkID)), 4, metadata->wav_fp);
    if(items_read != sizeof(char) * 4) return E_FILE_READ_FAIL;
    if(strncmp(ChunkID, CHUNK_ID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Chunk Size
    items_read = fread(&chunk_size, sizeof(typeof(chunk_size)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Format
    items_read = fread(Format, sizeof(typeof(*Format)), 4, metadata->wav_fp);
    if(items_read != (sizeof(char) * 4)) return E_FILE_READ_FAIL;
    if(strncmp(Format, FORMAT_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Sub Chunk 1 ID
    items_read = fread(SubChunk1ID, sizeof(typeof(*SubChunk1ID)), 4, metadata->wav_fp);
    if(items_read != 4) return E_FILE_READ_FAIL;
    if(strncmp(SubChunk1ID, SUB_CHUNK_1_ID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Sub Chunk 1 Size
    items_read = fread(&sub_chunk_1_size, sizeof(typeof(sub_chunk_1_size)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Audio Format
    items_read = fread(&metadata->audio_format, sizeof(typeof(metadata->audio_format)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Num Channels
    items_read = fread(&metadata->num_channels, sizeof(typeof(metadata->num_channels)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Sample Rate
    items_read = fread(&metadata->sample_rate, sizeof(typeof(metadata->sample_rate)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //byte_rate
    items_read = fread(&metadata->byte_rate, sizeof(typeof(metadata->byte_rate)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Block Align
    items_read = fread(&metadata->block_align, sizeof(typeof(metadata->block_align)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Bits Per Sample
    items_read = fread(&metadata->bits_per_sample, sizeof(typeof(metadata->bits_per_sample)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //NOTE: This could be a topic for further research...
    //      Not today though
    if(sub_chunk_1_size != 16)
        return E_NON_PCM_DATA;

    //Sub Chunk 2 ID
    items_read = fread(SubChunk2ID, sizeof(typeof(*SubChunk2ID)), 4, metadata->wav_fp);
    if(items_read != (sizeof(char) * 4)) return E_FILE_READ_FAIL;
    if(strncmp(SubChunk2ID, SUB_CHUNK_2_ID_C, 4)) return E_MALFORMED_HEADER; //Check that the ID isn't malformed

    //Sub Chunk 2 Size
    items_read = fread(&sub_chunk_2_size, sizeof(typeof(sub_chunk_2_size)), 1, metadata->wav_fp);
    if(items_read != 1) return E_FILE_READ_FAIL;

    //Data Position (using items_read for error handling)
    items_read = fgetpos(metadata->wav_fp, &offset);
    if(items_read != 0) return E_FILE_POS_FAIL;
    metadata->data_pos = offset;

    uint16_t bytes_per_sample = metadata->bits_per_sample / 8;

    //Check Byte Rate Accuracy
    if(metadata->byte_rate !=
        (metadata->sample_rate * metadata->num_channels * bytes_per_sample))
        return E_MALFORMED_HEADER;

    //Check Block Align
    if(metadata->block_align !=
        metadata->num_channels * bytes_per_sample)
        return E_MALFORMED_HEADER;

    //Calculate Num Samples
    metadata->num_samples = sub_chunk_2_size /
        (metadata->num_channels * bytes_per_sample);

    return out;
}

//NOTE: This function will overwrite the data of any file that is passed to it
//      Required metadata for this function to succeed:
//          Sample Rate
//          Number of Samples
//          Bits per Sample
//          Audio Format (from int list of wav audio formats)
//          Number of Channels
error_code write_wav_metadata(wav_metadata* metadata) {
    error_code out = E_OK;

    size_t items_written;
    int32_t chunk_size = 0;
    int32_t sub_chunk_1_size = 0;
    int32_t sub_chunk_2_size = 0;

    int32_t byte_rate = 0;
    int16_t block_align = 0;

    if(metadata->wav_fp == 0) return E_FILE_NOT_OPENED;

    //Calculate Byte Rate
    byte_rate = metadata->sample_rate * metadata->num_channels * (metadata->bits_per_sample / 8);

    //Calculate Block Align
    block_align = metadata->num_channels * (metadata->bits_per_sample / 8);

    //Calculate Sub Chunk 1 Size
    switch(metadata->audio_format) {
        case 1:
            sub_chunk_1_size = 16;
            break;
        case 3:
            sub_chunk_1_size = 16;
            break;
        default:
            return E_NON_PCM_DATA;
    }

    //Calculate Sub Chunk 2 Size
    sub_chunk_2_size = metadata->num_samples * metadata->num_channels * (metadata->bits_per_sample / 8);

    //Calculate Chunk Size
    //NOTE: This algorith may need appended if I want to use ACIDized wav files
    //      This is accurate for bulk standard wav files though
    chunk_size = 4 + (8 + sub_chunk_1_size) + (8 + sub_chunk_2_size);

    //Chunk ID
    items_written = fwrite(CHUNK_ID_C, 1, 4, metadata->wav_fp);
    if(items_written != 4) return E_FILE_WRITE_FAIL;

    //Chunk Size
    items_written = fwrite(&chunk_size, sizeof(chunk_size), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Format
    items_written = fwrite(FORMAT_C, 1, 4, metadata->wav_fp);
    if(items_written != 4) return E_FILE_WRITE_FAIL;

    //Sub Chunk 1 ID
    items_written = fwrite(SUB_CHUNK_1_ID_C, 1, 4, metadata->wav_fp);
    if(items_written != 4) return E_FILE_WRITE_FAIL;

    //Sub Chunk 1 Size
    items_written = fwrite(&sub_chunk_1_size, sizeof(sub_chunk_1_size), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Sub Chunk 1 Data
    //Audio Format
    items_written = fwrite(&metadata->audio_format, sizeof(typeof(metadata->audio_format)), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Num Channels
    items_written = fwrite(&metadata->num_channels, sizeof(typeof(metadata->num_channels)), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Sample Rate
    items_written = fwrite(&metadata->sample_rate, sizeof(typeof(metadata->sample_rate)), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Byte Rate
    items_written = fwrite(&byte_rate, sizeof(typeof(byte_rate)), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Block Align
    items_written = fwrite(&block_align, sizeof(typeof(block_align)), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Bits Per Sample
    items_written = fwrite(&metadata->bits_per_sample, sizeof(typeof(metadata->bits_per_sample)), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

    //Sub Chunk 2 ID
    items_written = fwrite(SUB_CHUNK_2_ID_C, 1, 4, metadata->wav_fp);
    if(items_written != 4) return E_FILE_WRITE_FAIL;

    //Sub Chunk 2 Size
    items_written = fwrite(&sub_chunk_2_size, sizeof(typeof(sub_chunk_2_size)), 1, metadata->wav_fp);
    if(items_written != 1) return E_FILE_WRITE_FAIL;

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
    formatted_print(0, 4, "Chunk ID", (value_t){string, 4, CHUNK_ID_C, 0});
    formatted_print(8, 4, "Format", (value_t){string, 4, FORMAT_C, 0});
    formatted_print(12, 4, "Sub Chunk 1 ID", (value_t){string, 4, SUB_CHUNK_1_ID_C, 0});
    formatted_print(20, 2, "Audio Format (1 = Int, 3 = F32)", (value_t){integer, 0, 0, metadata->audio_format});
    formatted_print(22, 2, "Number of Channels", (value_t){integer, 0, 0, metadata->num_channels});
    formatted_print(24, 4, "Sample Rate (Hz)", (value_t){integer, 0, 0, metadata->sample_rate});
    formatted_print(28, 4, "Byte Rate (Hz)", (value_t){integer, 0, 0, metadata->byte_rate});
    formatted_print(32, 2, "Block Align (B)", (value_t){integer, 0, 0, metadata->block_align});
    formatted_print(34, 2, "Bits Per Sample (b)", (value_t){integer, 0, 0, metadata->bits_per_sample});
    formatted_print(36, 4, "Sub Chunk 2 ID", (value_t){string, 4, SUB_CHUNK_2_ID_C, 0});
    formatted_print(INT64_MAX, SIZE_MAX, "Number of Samples", (value_t){integer, 0, 0, metadata->num_samples});
    formatted_print(INT64_MAX, SIZE_MAX, "Data Chunk Position", (value_t){integer, 0, 0, metadata->data_pos});
}
