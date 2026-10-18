# RecoverX Experiment Plan

## 1. Objective

Evaluate how different integrity-checking methods detect changes in file data and compare the storage required by deduplication against full-copy backups.

## 2. Experiment E1: Corruption Detection

### Methods to compare

- Simple sum
- Internet checksum
- CRC-32
- SHA-256

### Corruption modes

- `bitflip`
- `burst`
- `swap`
- `truncate`
- `zero`
- `deletechunk`

### Procedure

1. Generate or prepare test data.
2. Calculate and save the original integrity value for each method.
3. Apply one corruption mode to a test copy.
4. Recalculate the integrity value.
5. Compare the new value with the original value.
6. Record whether the corruption was detected.
7. Repeat the trials and calculate the detection rate.

The planned target is 1,000 trials per corruption mode. Results must be measured by the implemented experiment rather than assumed in advance.

### CSV output

Suggested columns:

`trial_id,method,corruption_mode,detected,data_size_bytes`

## 3. Experiment E2: Storage Efficiency

### Objective

Compare storing complete file copies with storing deduplicated chunks.

### Measurements

- Original data size in bytes
- Number of versions
- Full-copy storage size in bytes
- Deduplicated storage size in bytes
- Bytes saved
- Percentage of storage saved

### CSV output

Suggested columns:

`file_id,version_count,original_bytes,full_copy_bytes,deduplicated_bytes,bytes_saved,storage_saved_percent`

### Calculation

Bytes saved:

`full_copy_bytes - deduplicated_bytes`

Storage saved percentage:

`(bytes_saved / full_copy_bytes) * 100`

If `full_copy_bytes` is zero, the percentage should be reported as not applicable rather than dividing by zero.

## 4. Reporting

The final report should include:

- Experimental setup and test data
- Number of trials performed
- Measured detection rates by method and corruption mode
- Storage usage comparison
- Graphs generated from recorded CSV data
- Limitations and observations

No experimental results should be claimed until the experiments have actually been executed.