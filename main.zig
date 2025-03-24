const std = @import("std");

const tagged_value = enum {
    string,
    integer,
};

const value_type = union(tagged_value){
    string: []u8,
    integer: i32,
};

fn formatted_print(offset: u64, bytes: usize, identifier: []const u8, value: value_type) !void {
    //"Offset  | Bytes   | Identifier      | Value   "
    const writer = std.io.getStdOut().writer();
    try writer.print("{d: <8} | ", .{offset});
    try writer.print("{d: <8} | ", .{bytes});
    try writer.print("{s: <32} | ", .{identifier});
    switch(value) {
        .string => |val| try writer.print("{s: <8}", .{val}),
        .integer => |val| {
            if(val > 0) {
                const new_val: u32 = @intCast(val);
                try writer.print("{d: <8}", .{new_val});
            } else {
                try writer.print("{d: <8}", .{val});
            }
        },
    }
    try writer.print("\n", .{});
}

pub fn main() !void {
    const input_file = std.fs.cwd().openFile("test.wav", .{}) catch |err| {
        std.log.err("Error: {s}", .{@errorName(err)});
        return;
    };
    defer input_file.close();
    const reader = std.fs.File.reader(input_file);

    //Key
    try std.io.getStdOut().writer().print("{s: <8} | {s: <8} | {s: <32} | {s: <16}\n", .{"Offset", "Bytes", "Identifier", "Value"});
    var offset: u64 = undefined;
    var identifier: []const u8 = undefined;

    //RIFF ChunkID
    var ChunkID: [4]u8 = undefined;
    identifier = "ChunkID";
    offset = try input_file.getPos();
    var num_read = reader.readAll(&ChunkID) catch |err| {
        std.log.err("Error reading file: {s}", .{@errorName(err)});
        return;
    };
    try formatted_print(offset, num_read, identifier, .{ .string = &ChunkID });

    //ChunkSize (Rest of the file size / entire file size -8 bytes)
    var ChunkSizeBytes: [4]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "ChunkSize (Bytes until EOF)";
    num_read = reader.readAll(&ChunkSizeBytes) catch |err| {
        std.log.err("Error reading file: {s}", .{@errorName(err)});
        return;
    };
    const ChunkSize: i32 = @bitCast(ChunkSizeBytes);
    //This next line may be important for those intel users among us...
    //ChunkSize = @byteSwap(ChunkSize);
    try formatted_print(offset, num_read, identifier, .{ .integer = ChunkSize });
    identifier = "Total Size of File (Bytes)";
    try formatted_print(offset, 0, identifier, .{ .integer = (ChunkSize + 8) });

    //Format chunk (Spells out "WAVE" in ascii)
    var Format: [4]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "Format";
    num_read = reader.readAll(&Format) catch |err| {
        std.log.err("Error reading file: {s}", .{@errorName(err)});
        return;
    };
    try formatted_print(offset, num_read, identifier, .{ .string = &Format });

    //SubChunk1ID (Spells out "fmt " in ascii);
    var SubChunk1ID: [4]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "SubChunk1ID";
    num_read = reader.readAll(&SubChunk1ID) catch |err| {
        std.log.err("Error reading file: {s}", .{@errorName(err)});
        return;
    };
    try formatted_print(offset, num_read, identifier, .{ .string = &SubChunk1ID });

    //SubChunk1ID (Spells out "fmt " in ascii);
    var SubChunk1SizeBytes: [4]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "SubChunk1Size";
    num_read = reader.readAll(&SubChunk1SizeBytes) catch |err| {
        std.log.err("Error reading file: {s}", .{@errorName(err)});
        return;
    };
    const SubChunk1Size: i32 = @bitCast(SubChunk1SizeBytes);
    try formatted_print(offset, num_read, identifier, .{ .integer = SubChunk1Size });

    //Audio Format
    var AudioFormatBytes: [2]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "Audio Format (PCM = 1, IEEE = 3)";
    num_read = reader.readAll(&AudioFormatBytes) catch |err| {
        std.log.err("Error reading file: {s}", .{@errorName(err)});
        return;
    };
    const AudioFormat: i16 = @bitCast(AudioFormatBytes);
    try formatted_print(offset, num_read, identifier, .{ .integer = AudioFormat });

    //Number of Channels
    var NumChannelsBytes: [2]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "Number of Channels";
    num_read = try reader.readAll(&NumChannelsBytes);
    const NumChannels: i16 = @bitCast(NumChannelsBytes);
    try formatted_print(offset, num_read, identifier, .{ .integer = NumChannels });

    //Sample Rate
    var SampleRateBytes: [4]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "Sample Rate (Hz)";
    num_read = try reader.readAll(&SampleRateBytes);
    const SampleRate: i32 = @bitCast(SampleRateBytes);
    try formatted_print(offset, num_read, identifier, .{ .integer = SampleRate });

    //Byte Rate
    var ByteRateBytes: [4]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "Byte Rate (Hz)";
    num_read = try reader.readAll(&ByteRateBytes);
    const ByteRate: i32 = @bitCast(ByteRateBytes);
    try formatted_print(offset, num_read, identifier, .{ .integer = ByteRate });

    //Block Align
    var BlockAlignBytes: [2]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "Block Align";
    num_read = try reader.read(&BlockAlignBytes);
    const BlockAlign: i16 = @bitCast(BlockAlignBytes);
    try formatted_print(offset, num_read, identifier, .{ .integer = BlockAlign });

    //Bits Per Sample
    var BitsPerSampleBytes: [2]u8 = undefined;
    offset = try input_file.getPos();
    identifier = "Bits Per Sample";
    num_read = try reader.readAll(&BitsPerSampleBytes);
    const BitsPerSample: i16 = @bitCast(BitsPerSampleBytes);
    try formatted_print(offset, num_read, identifier, .{ .integer = BitsPerSample });

    if(AudioFormat != 1 and AudioFormat != 3) {
        try std.io.getStdOut().writer().print("Audio Format Is Not A PCM or IEEE754 Format ... Exiting.", .{});
        return;
    }
}
