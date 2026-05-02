# LiteRTDemo Windows Release (v1.0)

This is a pre-packaged release of the LiteRT-LM + UMG MCP Demo.

## 🚀 Installation Instructions

### 1. Download the Model
The large model file is excluded from this package. Please download it from:
- **Model**: `gemma-4-E2B-it.litertlm`
- **Download Link**: [https://huggingface.co/google/gemma-2-2b-it-tf-lite](https://huggingface.co/google/gemma-2-2b-it-tf-lite) (or your private Winyunq storage)

### 2. Deployment Path

#### For Windows Pre-compiled Build (`LiteRTDemo.exe`)
Extract this release and place the model file in:
`LiteRTDemo/Content/Models/gemma-4-E2B-it.litertlm`

#### For Source Build (C++ Compilation)
If you are compiling from source, place the model file in:
`[ProjectRoot]/Content/Models/gemma-4-E2B-it.litertlm`

## 🛠 Features
- High-performance local LLM inference via LiteRT.
- PreDefault loading for seamless integration.
- Custom UMG Chat UI with MCP capabilities.
