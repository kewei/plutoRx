#include <iostream>
#include <stdlib.h>
#include <vector>
#include <fftw3.h>
#include <complex>
#include "iio.h"
//#include "qcustomplot.h"

struct iio_channel *rx0_i, *rx0_q;
struct iio_buffer *rxbuf;
struct iio_context *ctx;
bool stop = false;
const int BUFFER_SIZE = 1024000;
const int output_size = (BUFFER_SIZE/2 + 1);

/* cleanup and exit */
static void shutdown()
{
	printf("* Destroying buffers\n");
	if (rxbuf) { iio_buffer_destroy(rxbuf); }

	printf("* Disabling streaming channels\n");
	if (rx0_i) { iio_channel_disable(rx0_i); }
	if (rx0_q) { iio_channel_disable(rx0_q); }

	printf("* Destroying context\n");
	if (ctx) { iio_context_destroy(ctx); }
	exit(0);
}

int receive(struct iio_context *ctx)
{
    struct iio_device *dev;
    size_t nrx = 0;

    dev = iio_context_find_device(ctx, "cf-ad9361-lpc");

    std::cout << *"Initializing AD9361 IIO streaming channels" << std::endl; 
    rx0_i = iio_device_find_channel(dev, "voltage0", 0);
    rx0_q = iio_device_find_channel(dev, "voltage1", 0);

    std::cout << "Enabling IIO streaming channels" << std::endl;
    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);
    /* if (iio_channel_is_scan_element(rx0_i) && iio_channel_is_scan_element(rx0_q))
    {
        iio_channel_enable(rx0_i);
        iio_channel_enable(rx0_q);
    }
    else
    {
        std::cout << "Channel i or q is not available." << std::endl;
        abort();
    } */
    
    std::cout << "Create buffer." << std::endl;
    rxbuf = iio_device_create_buffer(dev, BUFFER_SIZE, false);
    if (!rxbuf)
    {
        perror("Could not create RX buffer");
        shutdown();
    }

    std::cout << "Starting IO streaming (Press Ctrl+C to cancel)" << std::endl;
    while (!stop){

        //auto *t_dat;
        ssize_t nbytes_rx;
        char *p_dat, *p_end;
        ptrdiff_t p_inc;

        int n = 0;
//        std::vector<int> q_rec(BUFFER_SIZE);
        
        nbytes_rx = iio_buffer_refill(rxbuf);
        fftw_complex* output_buffer = static_cast<fftw_complex*>(fftw_malloc(output_size * sizeof(fftw_complex)));
        int flags = FFTW_ESTIMATE;
        if (nbytes_rx < 0) {
            std::cout << "Error refilling buf." << std::endl;
            shutdown();
        }
        //auto *p_dat = iio_buffer_start(rxbuf);
        //auto *p_end = iio_buffer_end(rxbuf);
        p_inc = iio_buffer_step(rxbuf);
        p_end = (char *)iio_buffer_end(rxbuf);

        for (p_dat = (char *)iio_buffer_first(rxbuf, rx0_i); p_dat < p_end; p_dat += p_inc)
        {

            const int16_t real_ = ((int16_t*)p_dat)[0];  // Real (I)
            const int16_t imag_ = ((int16_t*)p_dat)[1]; // Imag (Q)

            ++n;

        }
        fftw_plan plan = fftw_plan_dft_1d(BUFFER_SIZE, rxbuf, output_buffer, flags);
        std::cout << n << '\n';
        nrx += nbytes_rx / iio_device_get_sample_size(dev);
        std::cout << "RX " << nrx/1e6 << " MSmp" << std::endl;
    }
    shutdown();
    return 0;
}

static void handle_sig(int sig)
{
    std::cout << "Waiting fo process to finish... Got signal " << sig << std::endl;
    stop = true;
}

void calculate_fft(char* in_data, int N)
{

}


int main(int argc, char **argv)
{

    struct iio_device *phy;
    
    signal(SIGINT, handle_sig);

    try {
        ctx = iio_create_context_from_uri("ip:192.168.2.1");
        if (!ctx)
            throw std::logic_error("No context is created.");
    }
    catch(std::logic_error err) {
        std::cout << err.what() << std::endl;
        abort();
    }

    try {
        int num_devices = iio_context_get_devices_count(ctx);
        if (!num_devices)
            throw std::logic_error("No devices.");
    }
    catch(std::logic_error err) {
        std::cout << err.what() << std::endl;
        abort();
    }

    phy = iio_context_find_device(ctx, "ad9361-phy");
    auto *rx_chn = iio_device_find_channel(phy, "altvoltage0", true);
    iio_channel_attr_write_longlong(rx_chn, "frequency", 2400000000); // RX LO frequency

    iio_channel_attr_write_longlong(
        iio_device_find_channel(phy, "voltage0", false),
        "sampling_frequency", 5000000
    ); // RX baseband rate

    std::cout << iio_device_get_name(phy) << std::endl;
    receive(ctx);

    shutdown();
    return 0;
}