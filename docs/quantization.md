# Quantization

The original full-precision model is left untouched.

When you run `--model quantize` you'll get a new separate file next to it.

```bash
asryx --model install large-v3-turbo
asryx --model quantize large-v3-turbo q5_0
asryx --model use large-v3-turbo-q5_0
```

Switching between them is just `asryx --model use <name>`, no re-download needed.

```text
Base Model File:       ~/.local/share/asryx/models/ggml-<family>.bin           (e.g. ggml-base.en.bin)
Quantized Model File:  ~/.local/share/asryx/models/ggml-<family>-<quant>.bin   (e.g. ggml-base.en-q4_0.bin)
```

Supported schemes:

```text
q4_0, q4_1, q5_0, q5_1, q8_0
q2_k, q3_k, q4_k, q5_k, q6_k
```

The `q<N>_0` / `q<N>_1` family is the legacy GGML round-to-nearest scheme, weights are grouped into small blocks, each block gets a scale (and for `_1` variants, a min offset), and each weight is packed into N bits. Lower N means smaller and faster, at the cost of more quantization error per block.

The `q<N>_k` family (k-quants) uses a more elaborate block structure, nested super-blocks with per-block and per-super-block scaling,which generally gives better accuracy per bit than the legacy scheme at a comparable size. 
