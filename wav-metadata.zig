//TODO: Add function to read metadata from a wav file
//      Add function to write metadata to a new wav file
const std = @import("std");

//NOTE: The const fields in the wav metadata are used for verification of any
//      wav files being read and for writing the header for wav files being created
const wav_metadata = struct {
    ChunkID: []const u8 = "RIFF",
    ChunkSize: i32,
    Format: []const u8 = "WAVE",
    SubChunk1ID: []const u8 = "fmt ",
    SubChunk1Size: i32,
    AudioFormat: i16,       //This should be 1 for PCM (Integer) or 3 for IEEE754 (floating point)
    NumChannels: i16,
    SampleRate: i32,
    ByteRate: i32,          //This can be calculated as SampleRate * NumChannels * BitsPerSample/8
    BlockAlign: i16,        //This can be calculated as NumChannels * BitsPerSample/8 
                            //(Number of Bytes for one sample from all channels)
    BitsPerSample: i16,

    ExtraParamSize: ?i16,   //These ExtraParams are for Non PCM or IEEE754 data (probably will not be used)
    ExtraParams: ?[]u8,

    SubChunk2ID: []const u8 = "data",
    SubChunk2Size: i32,     //This is equal to NumSamples * NumChannels * BitsPerSample/8 
                            //(and should be set to 0 until the file is done being written, then calculated)
    DataPos: u64,           //This is a pointer to the position of the beginning of the data block of the file
};

//TODO: this
fn read_metadata(file: std.fs.File) !wav_metadata {
    var metadata_out: wav_metadata = undefined;
    const reader = std.fs.File.reader(file);
    var num_read: usize = undefined;

    //ChunkID [Bytes 0-3]
    var ChunkID: [4]u8 = undefined;
    num_read = try reader.readAll(&ChunkID);
    if(std.mem.eql(u8, &ChunkID, metadata_out.ChunkID)) { return error.InvalidMetadata; }

    //ChunkSize [Bytes 4-7]
    var ChunkSize: [4]u8 = undefined;
    num_read = try reader.readAll(&ChunkSize);
    metadata_out.ChunkSize = @bitCast(ChunkSize);

    //Format [Bytes 8-11]
    var Format: [4]u8 = undefined;
    num_read = try reader.readAll(&Format);
    if(std.mem.eql(u8, &Format, metadata_out.Format)) return error.InvalidMetadata;

    //SubChunk1ID [Bytes 12-15]
    var SubChunk1ID: [4]u8 = undefined;
    num_read = try reader.readAll(&SubChunk1ID);
    if(std.mem.eql(u8, &SubChunk1ID, metadata_out.SubChunk1ID)) return error.InvalidMetadata;

    //SubChunk1Size [Bytes 16-19]
    var SubChunk1Size: [4]u8 = undefined;
    num_read = try reader.readAll(&SubChunk1Size);
    metadata_out.SubChunk1Size = @bitCast(SubChunk1Size);

    //Audio Format [Bytes 20-21]
    var AudioFormat: [2]u8 = undefined;
    num_read = try reader.readAll(&AudioFormat);
    metadata_out.AudioFormat = @bitCast(AudioFormat);

    //Num Channels [Bytes 22-23]
    var NumChannels: [2]u8 = undefined;
    num_read = try reader.readAll(&NumChannels);
    metadata_out.NumChannels = @bitCast(NumChannels);

    //Sample Rate [Bytes 24-27]
    var SampleRate: [4]u8 = undefined;
    num_read = try reader.readAll(&SampleRate);
    metadata_out.SampleRate = @bitCast(SampleRate);

    //Byte Rate [Bytes 28-31]
    var ByteRate: [4]u8 = undefined;
    num_read = try reader.readAll(&ByteRate);
    metadata_out.ByteRate = @bitCast(ByteRate);

    //Block Align [Bytes 32-33]
    var BlockAlign: [2]u8 = undefined;
    num_read = try reader.readAll(&BlockAlign);
    metadata_out.BlockAlign = @bitCast(BlockAlign);

    //Bits Per Sample [Bytes 34-35]
    var BitsPerSample: [2]u8 = undefined;
    num_read = try reader.readAll(&BitsPerSample);
    metadata_out.BitsPerSample = @bitCast(BitsPerSample);

    //WARN: Not sure how to test this, I don't have any wav files of non PCM or IEEE754 data
    if(metadata_out.SubChunk1Size != 16) {
        //Data is not PCM, put the rest of the subchunk 1 data in extra params
        const extra_data_size: i16 = @intCast(metadata_out.SubChunk1Size - (16 + 2));

        //Extra Param Size
        var ExtraParamSize: [2]u8 = undefined;
        num_read = try reader.readAll(&ExtraParamSize);
        metadata_out.ExtraParamSize = @bitCast(ExtraParamSize);
        if(metadata_out.ExtraParamSize != extra_data_size) return error.MalformedExtraParams;

        //Extra Params
        const ExtraParams: []u8 = undefined;
        num_read = try reader.readAll(ExtraParams);
        metadata_out.ExtraParams = ExtraParams;
    }

    //Sub Chunk 2 ID [Bytes 36-39 ( + 2 + ExtraParamSize)]
    var SubChunk2ID: [4]u8 = undefined;
    num_read = try reader.readAll(&SubChunk2ID);
    if(std.mem.eql(u8, metadata_out.SubChunk2ID, &SubChunk2ID)) return error.InvalidMetadata;

    //Sub Chunk 2 Size [Bytes 40-43 ( + 2 + ExtraParamSize)]
    var SubChunk2Size: [4]u8 = undefined;
    num_read = try reader.readAll(&SubChunk2Size);
    metadata_out.SubChunk2Size = @bitCast(SubChunk2Size);

    //Verification on metadata fields
    //Byte Rate Verification
    if(metadata_out.ByteRate != 
        (metadata_out.SampleRate * metadata_out.NumChannels * (@divExact(metadata_out.BitsPerSample, 8)))
        ) return error.MalformedMetadata;

    //Block Align Verification
    if(metadata_out.BlockAlign !=
        (metadata_out.NumChannels * (@divExact(metadata_out.BitsPerSample, 8)))
        ) return error.MalformedMetadata;

    metadata_out.DataPos = try file.getPos();

    return metadata_out;
}

//TODO: Modify the return semantics of `read_metadata()`, currently the returned struct
//      loses the values of its parameters (specifically []const u8), adding @memcpy to a
//      parameterized output could fix this issue?
pub fn main() !void {

    const writer = std.io.getStdOut().writer();
    const input_file = try std.fs.cwd().openFile("test.wav", .{});
    defer input_file.close();

    const md = read_metadata(input_file) catch |err| {
        std.log.err("Error reading metadata: {s}", .{@errorName(err)});
        return;
    };

    try writer.print("ChunkID: {s}\n", .{md.ChunkID});
    try writer.print("ChunkSize: {d}\n", .{md.ChunkSize});
    try writer.print("Format: {s}\n", .{md.Format});
    try writer.print("SubChunk1ID: {s}\n", .{md.SubChunk1ID});
    try writer.print("SubChunk1Size: {d}\n", .{md.SubChunk1Size});
    try writer.print("AudioFormat: {d}\n", .{md.AudioFormat});
    try writer.print("NumChannels: {d}\n", .{md.NumChannels});
    try writer.print("SampleRate: {d}\n", .{md.SampleRate});
    try writer.print("ByteRate: {d}\n", .{md.ByteRate});
    try writer.print("BlockAlign: {d}\n", .{md.BlockAlign});
    try writer.print("BitsPerSample: {d}\n", .{md.BitsPerSample});
    try writer.print("SubChunk2ID: {s}\n", .{md.SubChunk2ID});
    try writer.print("SubChunk2Size: {d}\n", .{md.SubChunk2Size});
    try writer.print("DataPos: {d}\n", .{md.DataPos});
}

//TODO: this
//This function should return an error if the file is not empty and/or the file cursor is
//not at index 0
//fn write_metadata(file: std.fs.File, metadata: wav_metadata) !void {

//}
