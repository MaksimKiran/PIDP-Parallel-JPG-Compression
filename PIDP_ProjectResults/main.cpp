#define _CRT_SECURE_NO_WARNINGS

#include <mpi.h>
#include <stdio.h>
#include <jpeglib.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>

// Change here when testing
const char* INPUT_IMAGE = "2500x2500.jpg";


int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    std::cout << "[Rank " << rank << "] MPI initialized, total ranks: " << size << std::endl;

    const char* input_filename = INPUT_IMAGE;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* image_data = nullptr;

    // Rank 0 loads imagwe
    if (rank == 0) {
        std::cout << "[Rank 0] Loading image: " << input_filename << "\n" << std::flush;

        FILE* infile = nullptr;
        fopen_s(&infile, input_filename, "rb");
        if (!infile) {
            std::cerr << "[Rank 0] ERROR: Cannot open input file: " << input_filename << "\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        jpeg_decompress_struct cinfo;
        jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);
        jpeg_stdio_src(&cinfo, infile);
        jpeg_read_header(&cinfo, TRUE);
        jpeg_start_decompress(&cinfo);

        width = cinfo.output_width;
        height = cinfo.output_height;
        channels = cinfo.output_components;

        std::cout << "[Rank 0] Image dimensions: " << width << "x" << height << "x" << channels << "\n" << std::flush;

        image_data = new unsigned char[width * height * channels];
        JSAMPROW row_pointer[1];
        for (int y = 0; y < height; y++) {
            row_pointer[0] = image_data + y * width * channels;
            jpeg_read_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);

        std::cout << "[Rank 0] Image loaded successfully.\n" << std::flush;
    }

    // Broadcast image dimensions
    std::cout << "[Rank " << rank << "] Broadcasting dimensions...\n" << std::flush;
    MPI_Bcast(&width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&height, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&channels, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::cout << "[Rank " << rank << "] Received: " << width << "x" << height << "x" << channels << "\n" << std::flush;

    // Prepare scatter counts
    int base_rows = height / size;
    int remainder = height % size;

    std::vector<int> sendcounts(size);
    std::vector<int> displs(size);

    int offset = 0;
    for (int r = 0; r < size; r++) {
        int rows = base_rows + (r < remainder ? 1 : 0);
        sendcounts[r] = rows * width * channels;
        displs[r] = offset;
        offset += sendcounts[r];
    }

    int local_bytes = sendcounts[rank];
    std::vector<unsigned char> local_data(local_bytes);

    std::cout << "[Rank " << rank << "] About to scatter, local_bytes=" << local_bytes << "\n" << std::flush;

    MPI_Scatterv(image_data, sendcounts.data(), displs.data(), MPI_UNSIGNED_CHAR,
                 local_data.data(), local_bytes, MPI_UNSIGNED_CHAR,
                 0, MPI_COMM_WORLD);

    std::cout << "[Rank " << rank << "] Scatter complete!\n" << std::flush;

    // Parallel
    auto start_parallel = std::chrono::high_resolution_clock::now();

    char out_filename[256];
    sprintf_s(out_filename, "output_rank_%d.jpg", rank);

    std::cout << "[Rank " << rank << "] Opening output file: " << out_filename << "\n" << std::flush;
    FILE* outfile = nullptr;
    fopen_s(&outfile, out_filename, "wb");
    if (!outfile) {
        std::cerr << "[Rank " << rank << "] Cannot open output file!\n";
        MPI_Abort(MPI_COMM_WORLD, 2);
    }

    std::cout << "[Rank " << rank << "] Creating compressor...\n" << std::flush;
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    int local_rows = local_bytes / (width * channels);
    cinfo.image_width = width;
    cinfo.image_height = local_rows;
    cinfo.input_components = channels;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);

    std::cout << "[Rank " << rank << "] Starting compression of " << local_rows << " rows...\n" << std::flush;
    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    for (int y = 0; y < local_rows; y++) {
        row_pointer[0] = local_data.data() + y * width * channels;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);

    auto end_parallel = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_parallel - start_parallel;
    double local_time = elapsed.count();
    std::cout << "[Rank " << rank << "] Compression done! Time: " << local_time << "s\n" << std::flush;

    // Gather all times to rank 0
    double total_parallel_time = 0.0;
    MPI_Reduce(&local_time, &total_parallel_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Sequential compression (0)
    if (rank == 0) {
        std::cout << "\n[Rank 0] Starting sequential compression...\n" << std::flush;
        auto start_seq = std::chrono::high_resolution_clock::now();

        FILE* outfile_seq = nullptr;
        fopen_s(&outfile_seq, "output_sequential.jpg", "wb");
        if (!outfile_seq) {
            std::cerr << "[Rank 0] Cannot open sequential output file!\n";
            MPI_Abort(MPI_COMM_WORLD, 3);
        }

        jpeg_compress_struct cinfo_seq;
        jpeg_error_mgr jerr_seq;
        cinfo_seq.err = jpeg_std_error(&jerr_seq);
        jpeg_create_compress(&cinfo_seq);
        jpeg_stdio_dest(&cinfo_seq, outfile_seq);

        cinfo_seq.image_width = width;
        cinfo_seq.image_height = height;
        cinfo_seq.input_components = channels;
        cinfo_seq.in_color_space = JCS_RGB;

        jpeg_set_defaults(&cinfo_seq);
        jpeg_set_quality(&cinfo_seq, 90, TRUE);
        jpeg_start_compress(&cinfo_seq, TRUE);

        JSAMPROW row_pointer_seq[1];
        for (int y = 0; y < height; y++) {
            row_pointer_seq[0] = image_data + y * width * channels;
            jpeg_write_scanlines(&cinfo_seq, row_pointer_seq, 1);
        }

        jpeg_finish_compress(&cinfo_seq);
        jpeg_destroy_compress(&cinfo_seq);
        fclose(outfile_seq);

        auto end_seq = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seq = end_seq - start_seq;
        double sequential_time = elapsed_seq.count();

        std::cout << "\n=== TIMING COMPARISON ===\n";
        std::cout << "Parallel compression time (wall clock): " << total_parallel_time << " seconds\n";
        std::cout << "Sequential compression time: " << sequential_time << " seconds\n";
        std::cout << "Speedup: " << (sequential_time / total_parallel_time) << "x\n";
        std::cout << "Efficiency: " << (sequential_time / total_parallel_time / size * 100.0) << "%\n";
        std::cout << "\nOutput files:\n";
        std::cout << "  - output_sequential.jpg (full image)\n";
        std::cout << "  - output_rank_0.jpg through output_rank_" << (size-1) << ".jpg (chunks)\n";
    }

    if (image_data) delete[] image_data;

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "\n=== ALL RANKS COMPLETED SUCCESSFULLY ===\n";
    }

    MPI_Finalize();
    std::cout << "[Rank " << rank << "] Finalized.\n" << std::flush;
    return 0;
}