#include "unity.h"
#include "boards.h"
#include "tune_filter.h"
#include "eval/tune_export.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_tuning_filter_and_export(void)
{
    // 1. Create a mock EPD input file
    const char *mock_epd_in = "temp_mock_input.epd";
    const char *mock_epd_out = "temp_mock_filtered.epd";
    const char *mock_csv_out = "temp_mock_features.csv";

    FILE *f = fopen(mock_epd_in, "w");
    TEST_ASSERT_NOT_NULL(f);
    
    // We write a quiet position (startpos) and a non-quiet position
    // First line: quiet startpos. Note that it needs "score X" token in it.
    fprintf(f, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 score 10\n");
    // Second line: invalid or check/tactical position that is not quiet
    // or just another quiet position with score -150 to verify simulated outcomes
    fprintf(f, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1 score -200\n");
    fclose(f);

    // Run the filter
    int rc = run_tune_filter(mock_epd_in, mock_epd_out);
    TEST_ASSERT_EQUAL_INT(0, rc);

    // Verify filtered file exists and can be parsed by the exporter
    FILE *f_filtered = fopen(mock_epd_out, "r");
    TEST_ASSERT_NOT_NULL(f_filtered);
    
    char line[16384];
    int lines_count = 0;
    while (fgets(line, sizeof(line), f_filtered)) {
        lines_count++;
        // Output should be: <FEN> | <simulated_result> | <white_score>
        TEST_ASSERT_NOT_NULL(strstr(line, "|"));
    }
    fclose(f_filtered);
    TEST_ASSERT_TRUE(lines_count > 0);

    // Run the exporter on the filtered file
    rc = run_tune_export(mock_epd_out, mock_csv_out);
    TEST_ASSERT_EQUAL_INT(0, rc);

    // Verify CSV file is created and has valid headers and rows
    FILE *f_csv = fopen(mock_csv_out, "r");
    TEST_ASSERT_NOT_NULL(f_csv);
    
    int csv_lines = 0;
    while (fgets(line, sizeof(line), f_csv)) {
        csv_lines++;
        if (csv_lines == 1) {
            // Check headers start with result,const_score
            TEST_ASSERT_EQUAL_INT(0, strncmp(line, "result,const_score", 18));
        } else {
            // Check rows have comma-separated values
            TEST_ASSERT_NOT_NULL(strchr(line, ','));
        }
    }
    fclose(f_csv);
    TEST_ASSERT_EQUAL_INT(lines_count + 1, csv_lines); // Header + lines_count rows

    // Clean up temporary files
    remove(mock_epd_in);
    remove(mock_epd_out);
    remove(mock_csv_out);
}

int main(void)
{
    bitboard_init();
    UNITY_BEGIN();
    RUN_TEST(test_tuning_filter_and_export);
    return UNITY_END();
}
