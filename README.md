# FIRFilter

An audio plugin implementation of Finite Impulse Response (FIR) low-pass and high-pass filters. Built with C++ and the JUCE framework, this project demonstrates advanced digital signal processing (DSP) techniques for both filter design and real-time audio processing optimization.

## DSP Architecture & Features

This plugin goes beyond basic time-domain processing by implementing two distinct approaches to FIR filter design and utilizing FFT-based convolution for performance.

### 1. Filter Design Methods
* **Windowing Method:** Calculates filter coefficients mathematically using the ideal sinc function, truncated and smoothed using various windowing algorithms to mitigate the Gibbs phenomenon.
* **Frequency Sampling Method:** Allows for filter design by directly specifying the desired frequency response. It calculates the necessary time-domain impulse response by applying an **Inverse Fast Fourier Transform (IFFT)** to the sampled frequency bins.

### 2. Real-Time Processing Optimization
* **Frequency Domain Partitioned Convolution:** Standard time-domain convolution $O(N \cdot M)$ is highly inefficient for long FIR filters. To achieve zero-latency or low-latency real-time processing without overloading the CPU, this plugin implements partitioned convolution in the frequency domain using the Fast Fourier Transform (FFT).

### 3. Core Capabilities
* Switchable Low-Pass and High-Pass modes.
* Adjustable cutoff frequencies and filter lengths/orders.
* Selectable windowing functions for the time-domain design method.

## Technical Details

This project is developed as a seminar assignment for the Faculty of Electrical Engineering and Computing (FER). It leverages standard C++ audio processing paradigms within the JUCE framework to handle incoming audio buffers, state parameters, and complex FFT operations.

## Prerequisites

To build this project from source, you will need:
* **JUCE Framework:** Download and install [JUCE](https://juce.com/).
* **C++ Compiler:** MSVC (Windows), Clang (macOS), or GCC (Linux).
* **IDE:** Visual Studio, Xcode, or a similar C++ environment.

## Building the Plugin

1. Clone the repository to your local machine:
   ```bash
   git clone [https://github.com/andro2601/FIRFilter.git](https://github.com/andro2601/FIRFilter.git)

2. Navigate to the project directory and open the FIRFilter.jucer file in the Projucer application.

3. Verify that your target exporters (e.g., Visual Studio 2022) are correctly configured in Projucer.

4. Click "Save and Open in IDE".

5. Build the project within your IDE. The output will generate standard audio plugin formats (such as VST3, AU, or a Standalone application) depending on your Projucer settings.
