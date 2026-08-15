#include "test.hpp"

using namespace std;

vector<float> loadCSV(const string& filename)
{
    vector<float> data;

    ifstream file(filename);

    string line;

    while (getline(file, line))
    {
        stringstream ss(line);
        string value;

        while (getline(ss, value, ','))
        {
            data.push_back(stof(value));
        }
    }

    return data;
}

void saveRDMToCSV(
    const vector<complex<float>>& rdm,
    const string& filename_real,
    const string& filename_imag,
    size_t rx)
{
    ofstream real_file(filename_real);
    ofstream imag_file(filename_imag);

    for (size_t range_bin = 0; range_bin < 128; ++range_bin)
    {
        for (size_t doppler_bin = 0; doppler_bin < 64; ++doppler_bin)
        {
            size_t idx =
                (range_bin * 64 + doppler_bin) * 2 + rx;

            real_file << rdm[idx].real();
            imag_file << rdm[idx].imag();

            if (doppler_bin != 63)
            {
                real_file << ",";
                imag_file << ",";
            }
        }

        real_file << "\n";
        imag_file << "\n";
    }
}