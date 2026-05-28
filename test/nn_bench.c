#include "nn.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

static double now_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

int main(int argc, char **argv)
{
    int iterations = 10000;
    if (argc > 1) {
        iterations = atoi(argv[1]);
        if (iterations <= 0) {
            fprintf(stderr, "Iterations must be > 0\n");
            return 1;
        }
    }

    // Architecture: 768 -> 128 -> 1
    int sizes[] = {768, 128, 1};
    NeuralNetwork *nn = nn_init(sizes, 3);
    if (!nn) {
        fprintf(stderr, "Failed to initialize neural network\n");
        return 1;
    }

    // Generate random input vector
    float *inputs = malloc(sizes[0] * sizeof(float));
    for (int i = 0; i < sizes[0]; i++) {
        inputs[i] = (float)rand() / (float)RAND_MAX;
    }
    float target = 0.5f;
    float lr = 0.01f;

    printf("Benchmarking neural network: layers=[768, 128, 1], iterations=%d\n", iterations);
    
    double t0 = now_sec();
    
    // Mix forward propagation and training steps
    for (int i = 0; i < iterations; i++) {
        // Toggle input values slightly to simulate dynamic data
        inputs[i % sizes[0]] = (float)(i % 10) / 10.0f;
        
        nn_forward(nn, inputs);
        nn_train_step(nn, inputs, target, lr);
    }
    
    double t1 = now_sec();
    double elapsed = t1 - t0;
    
    printf("Time elapsed: %.6f s\n", elapsed);
    printf("Performance: %.2f iterations/s\n", (double)iterations / elapsed);

    free(inputs);
    nn_free(nn);
    return 0;
}
