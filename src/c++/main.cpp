#include "floating-point/lossy.hpp"
#include "floating-point/lossless.hpp"
#include "piecewise-approximation/constant.hpp"
#include "piecewise-approximation/linear.hpp"
#include "piecewise-approximation/polynomial.hpp"
#include "model-selection/model-selection.hpp"

using namespace std;
using namespace std::chrono;


Monitor Monitor::instance;
std::queue<high_resolution_clock::time_point> time_stream;

TimeSeries loadTimeseries(string input) {
    TimeSeries timeseries;
    CSVObj* head_obj = BatchIO::readCSV(input);
    CSVObj* curr_obj = head_obj;

    while (curr_obj != nullptr) {
        time_t time = (time_t) stol(curr_obj->getData(0));
        float value = stof(curr_obj->getData(1));        
        timeseries.push(new Univariate(time, value));

        curr_obj = (CSVObj*) curr_obj->getNext();
    }

    IOObj::clear(head_obj);
    return timeseries;
}


int main(int argc, char** argv) {
    const string INPUT = argv[1];
    const string COM_OUTPUT = argv[2];
    const string DECOM_OUTPUT = argv[3];
    const int INTERVAL = stoi(argv[4]);
    const string ALGO = argv[5];

    TimeSeries data_stream = loadTimeseries(INPUT);
    std::pair<long, long> memory_baseline = Monitor::getMemory();
    std::fstream memory_baseline_file(".memory_baseline", std::ios::out);
    memory_baseline_file << memory_baseline.first << "," << memory_baseline.second << "\n";
    memory_baseline_file.close();

    Monitor::instance.start(".mon");
    
    BaseDecompression* decompressor = nullptr;
    BaseCompression* compressor = nullptr;

    if (ALGO == "gorilla") {
        compressor = new Gorilla::Compression(COM_OUTPUT);
        decompressor = new Gorilla::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time());
    }
    else if (ALGO == "chimp") {
        compressor = new Chimp::Compression(COM_OUTPUT);
        decompressor = new Chimp::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time());
    }
    else if (ALGO == "elf") {
        compressor = new Elf::Compression(COM_OUTPUT);
        decompressor = new Elf::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time());
    }
    else if (ALGO == "self") {
        compressor = new SElf::Compression(COM_OUTPUT);
        decompressor = new SElf::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time());
    }
    else if (ALGO == "serf") {
        compressor = new Serf::Compression(COM_OUTPUT);
        decompressor = new Serf::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time());
    }
    else if (ALGO == "camel") {
        compressor = new Camel::Compression(COM_OUTPUT);
        decompressor = new Camel::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time());
    }
    else if (ALGO == "pmc") {
        compressor = new PMC::Compression(COM_OUTPUT);       
        decompressor = new PMC::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "hybrid-pca") {
        compressor = new HybridPCA::Compression(COM_OUTPUT);
        decompressor = new HybridPCA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "cov-pla") {
        compressor = new CovariancePLA::Compression(COM_OUTPUT);
        decompressor = new CovariancePLA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "swing-filter") {
        compressor = new SwingFilter::Compression(COM_OUTPUT);
        decompressor = new SwingFilter::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "slide-filter") {
        compressor = new SlideFilter::Compression(COM_OUTPUT);
        decompressor = new SlideFilter::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "optimal-pla") {
        compressor = new OptimalPLA::Compression(COM_OUTPUT);
        decompressor = new OptimalPLA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "conn-I-pla") {
        compressor = new ConnIPLA::Compression(COM_OUTPUT);
        decompressor = new ConnIPLA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "ioriented-pla") {
        compressor = new IOrientedPLA::Compression(COM_OUTPUT);
        decompressor = new IOrientedPLA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "semi-optimal-pla") {
        compressor = new SemiOptimalPLA::Compression(COM_OUTPUT);
        decompressor = new SemiOptimalPLA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "semi-mixed-pla") {
        compressor = new SemiMixedPLA::Compression(COM_OUTPUT);
        decompressor = new SemiMixedPLA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "cached-normal-equation") {
        compressor = new CachedNormalEquation::Compression(COM_OUTPUT);
        decompressor = new CachedNormalEquation::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "mix-piece") {
        compressor = new MixPiece::Compression(COM_OUTPUT);
        decompressor = new MixPiece::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "adaptive-approximation") {
        compressor = new AdaptiveApproximation::Compression(COM_OUTPUT);
        decompressor = new AdaptiveApproximation::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "swab") {
        compressor = new Swab::Compression(COM_OUTPUT);
        decompressor = new Swab::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "poly-swab") {
        compressor = new PolySwab::Compression(COM_OUTPUT);
        decompressor = new PolySwab::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "smart-grid-compression") {
        compressor = new SmartGridCompression::Compression(COM_OUTPUT);
        decompressor = new SmartGridCompression::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }
    else if (ALGO == "adapt-ppa") {
        compressor = new AdaptPPA::Compression(COM_OUTPUT);
        decompressor = new AdaptPPA::Decompression(DECOM_OUTPUT, INTERVAL, ((Univariate*) data_stream.get(0))->get_time()); 
    }

    compressor->initialize(argc - 6, &argv[6]);
    decompressor->initialize(argc - 6, &argv[6]);

    long count = 0;
    long max_latency = -1e18;
    double sum_latency = 0;
    double d_total_time = 0;

    Clock com_clock;
    Clock decom_clock;
    std::vector<float> decom_times;

    while (data_stream.hasNext()) {
        Univariate* data = (Univariate*) data_stream.next();
        
        time_stream.push(high_resolution_clock::now());
        com_clock.tick();
        Monitor::instance.setCompression();
        BinObj* obj = compressor->process(data);
        Monitor::instance.setIdle();
        com_clock.tick();

        while (obj != nullptr) {
            high_resolution_clock::time_point curr_time = high_resolution_clock::now();
            decom_clock.start();
            Monitor::instance.setDecompression();
            long length = decompressor->process(obj);
            Monitor::instance.setIdle();
            long duration = decom_clock.stop();
            d_total_time += duration;

            if (length > 0 && time_stream.size() > 0) {
                decom_times.push_back((float) duration / length);
                for (int i=0; i<length; i++) {
                    long latency = duration_cast<nanoseconds>(curr_time - time_stream.front()).count();
                    sum_latency += latency;
                    max_latency = max_latency < latency ? latency : max_latency;
                    
                    time_stream.pop();
                }
            }
            
            count += length;
            obj = (BinObj*) obj->getNext();
        }
        compressor->clear_buffer();
    }

    // Finalizing
    Monitor::instance.setCompression();
    BinObj* obj = compressor->complete();
    Monitor::instance.setIdle();
    while (obj != nullptr) {
        high_resolution_clock::time_point curr_time = high_resolution_clock::now();
        decom_clock.start();
        Monitor::instance.setDecompression();
        long length = decompressor->process(obj);
        Monitor::instance.setIdle();
        long duration = decom_clock.stop();
        d_total_time += duration;

        if (length > 0) {
            decom_times.push_back((float) duration / length);
            for (int i=0; i<length; i++) {
                long latency = duration_cast<nanoseconds>(curr_time - time_stream.front()).count();
                sum_latency += latency;
                max_latency = max_latency < latency ? latency : max_latency;
                time_stream.pop();
            }
        }
        
        count += length;
        obj = (BinObj*) obj->getNext();
    }
    Monitor::instance.setDecompression();
    decompressor->complete();
    Monitor::instance.setIdle();
    data_stream.finalize();

    // Stop monitoring so the per-phase hardware-energy integral is final, then
    // read the real edge energy (mJ) the monitor attributed to each phase. When
    // no hardware energy source exists on this host these are 0 (never "N/A"),
    // keeping the downstream CSV columns aligned.
    Monitor::instance.stop();
    double c_energy = Monitor::instance.getCompressionEnergy();
    double d_energy = Monitor::instance.getDecompressionEnergy();

    // Profiling time
    std::cout << std::fixed << "Average compress time (ns): " << com_clock.getAvgDuration() << "\n";
    std::cout << std::fixed << "Average latency (ns): " << (sum_latency / count) << "\n";
    std::cout << std::fixed << "Max latency (ns): " << max_latency << "\n";
    std::cout << std::fixed << "Average decompress time (ns): " << ((float) std::accumulate(decom_times.begin(), decom_times.end(), 0.0) / decom_times.size()) << "\n";
    std::cout << std::fixed << "Max decompress time (ns): " << (*std::max_element(decom_times.begin(), decom_times.end())) << "\n";
    std::cout << "Energy meter: " << Monitor::instance.energySource() << "\n";
    std::cout << std::fixed << "Compress energy (mJ): " << c_energy << "\n";
    std::cout << std::fixed << "Decompress energy (mJ): " << d_energy << "\n";

    IterIO timeFile(".time", false);
    timeFile.write("Average compress time (ns): " + std::to_string(com_clock.getAvgDuration()));
    timeFile.write("Average latency (ns): " + std::to_string(sum_latency / count));
    timeFile.write("Max latency (ns): " + std::to_string(max_latency));
    timeFile.write("Average decompress time (ns): " + std::to_string(((float) std::accumulate(decom_times.begin(), decom_times.end(), 0.0) / decom_times.size())));
    timeFile.write("Max decompress time (ns): " + std::to_string(*std::max_element(decom_times.begin(), decom_times.end())));
    timeFile.write("Compress energy (mJ): " + std::to_string(c_energy));
    timeFile.write("Decompress energy (mJ): " + std::to_string(d_energy));
    timeFile.close();

    delete compressor;
    delete decompressor;

    return 0;
}
