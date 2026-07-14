// Standalone integration probe for the exact LiteRT-LM DLL shipped by this demo.
// It deliberately bypasses Unreal and exercises the stable LiteRtLm_GetApi ABI
// through the public header-only C++ facade.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "litert_lm.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string ToUtf8(std::wstring_view value) {
  if (value.empty()) return {};
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                       static_cast<int>(value.size()), nullptr,
                                       0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(),
                      static_cast<int>(value.size()), result.data(), size,
                      nullptr, nullptr);
  return result;
}

std::string JsonEscape(std::string_view value) {
  std::ostringstream out;
  for (const unsigned char c : value) {
    switch (c) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          const char* hex = "0123456789ABCDEF";
          out << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
        } else {
          out << static_cast<char>(c);
        }
    }
  }
  return out.str();
}

std::string Q(std::string_view value) {
  return "\"" + JsonEscape(value) + "\"";
}

class JsonlLog {
 public:
  explicit JsonlLog(const fs::path& path) : stream_(path, std::ios::binary) {}

  bool good() const { return stream_.good(); }

  void Event(std::string_view event, std::string_view fields = {}) {
    stream_ << "{\"event\":" << Q(event);
    if (!fields.empty()) stream_ << ',' << fields;
    stream_ << "}\n";
    stream_.flush();
  }

 private:
  std::ofstream stream_;
};

struct CheckState {
  int passed = 0;
  int failed = 0;
};

void Check(JsonlLog& log, CheckState& state, std::string_view name,
           bool pass, std::string_view detail = {}) {
  if (pass) {
    ++state.passed;
  } else {
    ++state.failed;
  }
  log.Event("check", "\"name\":" + Q(name) +
                         ",\"pass\":" + (pass ? "true" : "false") +
                         ",\"detail\":" + Q(detail));
  std::cout << (pass ? "[PASS] " : "[FAIL] ") << name;
  if (!detail.empty()) std::cout << " - " << detail;
  std::cout << '\n';
}

void LogStatus(JsonlLog& log, std::string_view event,
               const litertlm::Status& status) {
  log.Event(event,
            "\"ok\":" + std::string(status.ok() ? "true" : "false") +
                ",\"status\":" +
                std::to_string(static_cast<int>(status.code())) +
                ",\"phase\":" +
                std::to_string(static_cast<int>(status.phase())) +
                ",\"native_code\":" +
                std::to_string(status.native_code()) +
                ",\"retryable\":" +
                std::string(status.retryable() ? "true" : "false") +
                ",\"message\":" + Q(status.message()));
}

std::optional<litertlm::RequestResult> Ask(
    litertlm::LiteRtLm& context, JsonlLog& log, std::string_view label,
    std::string_view prompt, uint32_t max_output_tokens = 160) {
  const std::string memory_before = context.ExportMemoryJson();
  litertlm::AskOptions options;
  options.max_output_tokens = max_output_tokens;
  options.timeout = std::chrono::minutes(5);

  std::cout << "\n[ASK " << label << "] " << prompt << '\n';
  auto answer = context.Ask(prompt, options);
  if (!answer) {
    LogStatus(log, "ask_api_error", answer.status());
    std::cout << "[API ERROR] " << answer.status().message() << '\n';
    return std::nullopt;
  }

  const std::string memory_after = context.ExportMemoryJson();
  const auto& r = *answer;
  log.Event(
      "ask_result",
      "\"label\":" + Q(label) + ",\"prompt\":" + Q(prompt) +
          ",\"status\":" + std::to_string(static_cast<int>(r.status)) +
          ",\"finish_reason\":" +
          std::to_string(static_cast<int>(r.finish_reason)) +
          ",\"backend\":" +
          std::to_string(static_cast<int>(r.backend)) +
          ",\"input_tokens\":" + std::to_string(r.input_tokens) +
          ",\"output_tokens\":" + std::to_string(r.output_tokens) +
          ",\"context_tokens\":" + std::to_string(r.context_tokens) +
          ",\"conversation_generation\":" +
          std::to_string(r.conversation_generation) +
          ",\"text\":" + Q(r.text) +
          ",\"response_json\":" + Q(r.response_json) +
          ",\"memory_before\":" + Q(memory_before) +
          ",\"memory_after\":" + Q(memory_after));
  std::cout << "[TEXT] " << r.text << '\n';
  std::cout << "[RESPONSE_JSON] " << r.response_json << '\n';
  std::cout << "[MEMORY_SIZE] " << context.MemorySize() << '\n';
  return answer.TakeValue();
}

std::optional<litertlm::RequestResult> AskJson(
    litertlm::LiteRtLm& context, JsonlLog& log, std::string_view label,
    std::string_view messages_json, uint32_t max_output_tokens = 160) {
  const std::string memory_before = context.ExportMemoryJson();
  litertlm::AskOptions options;
  options.max_output_tokens = max_output_tokens;
  options.timeout = std::chrono::minutes(5);

  std::cout << "\n[ASK_JSON " << label << "] " << messages_json << '\n';
  auto answer = context.AskJson(messages_json, options);
  if (!answer) {
    LogStatus(log, "ask_json_api_error", answer.status());
    std::cout << "[API ERROR] " << answer.status().message() << '\n';
    return std::nullopt;
  }

  const std::string memory_after = context.ExportMemoryJson();
  const auto& r = *answer;
  log.Event(
      "ask_json_result",
      "\"label\":" + Q(label) +
          ",\"messages_json\":" + Q(messages_json) +
          ",\"status\":" + std::to_string(static_cast<int>(r.status)) +
          ",\"finish_reason\":" +
          std::to_string(static_cast<int>(r.finish_reason)) +
          ",\"text\":" + Q(r.text) +
          ",\"response_json\":" + Q(r.response_json) +
          ",\"memory_before\":" + Q(memory_before) +
          ",\"memory_after\":" + Q(memory_after));
  std::cout << "[TEXT] " << r.text << '\n';
  std::cout << "[RESPONSE_JSON] " << r.response_json << '\n';
  return answer.TakeValue();
}

bool Contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

void PrintUsage() {
  std::cout << "Usage: LiteRtLmConversationProbe.exe <model.litertlm> "
               "<Win64 DLL directory> <output.jsonl>\n";
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  if (argc != 4) {
    PrintUsage();
    return 2;
  }

  const fs::path model_path = argv[1];
  const fs::path dll_dir = argv[2];
  const fs::path log_path = argv[3];
  fs::create_directories(log_path.parent_path());
  JsonlLog log(log_path);
  if (!log.good()) {
    std::cerr << "Unable to create log: " << ToUtf8(log_path.wstring()) << '\n';
    return 2;
  }

  log.Event("probe_start", "\"model\":" + Q(ToUtf8(model_path.wstring())) +
                               ",\"dll_dir\":" + Q(ToUtf8(dll_dir.wstring())));

  const std::vector<std::wstring> dependency_names = {
      L"dxil.dll", L"dxcompiler.dll", L"libLiteRt.dll",
      L"libGemmaModelConstraintProvider.dll",
      L"libLiteRtTopKWebGpuSampler.dll",
      L"libLiteRtWebGpuAccelerator.dll"};
  std::vector<HMODULE> modules;
  for (const auto& name : dependency_names) {
    const fs::path path = dll_dir / name;
    HMODULE module = LoadLibraryW(path.c_str());
    if (module == nullptr) {
      log.Event("load_error", "\"path\":" + Q(ToUtf8(path.wstring())) +
                                  ",\"win32_error\":" +
                                  std::to_string(GetLastError()));
      std::cerr << "Failed to load " << ToUtf8(path.wstring()) << '\n';
      return 2;
    }
    modules.push_back(module);
  }

  const fs::path wrapper_path = dll_dir / L"litert_lm_wrapper.dll";
  HMODULE wrapper = LoadLibraryW(wrapper_path.c_str());
  if (wrapper == nullptr) {
    log.Event("load_error", "\"path\":" + Q(ToUtf8(wrapper_path.wstring())) +
                                ",\"win32_error\":" +
                                std::to_string(GetLastError()));
    return 2;
  }
  auto get_api = reinterpret_cast<litertlm::Api::GetApiFunction>(
      GetProcAddress(wrapper, "LiteRtLm_GetApi"));
  if (get_api == nullptr) {
    log.Event("load_error", "\"symbol\":\"LiteRtLm_GetApi\"");
    return 2;
  }

  const std::string model_utf8 = ToUtf8(model_path.wstring());
  litertlm::LiteRtLmOptions create_options;
  create_options.model_path = model_utf8;
  create_options.backend = litertlm::Backend::Gpu;
  create_options.max_context_tokens = 32000;
  create_options.sampling.temperature = 1.0f;
  create_options.sampling.top_p = 0.95f;
  create_options.sampling.top_k = 64;
  create_options.sampling.random_seed = true;
  create_options.native_benchmark_metrics = true;
  create_options.system_prompt =
      "你是底层API记忆诊断助手。严格依据当前会话历史回答，不要把不同会话的信息混在一起。";

  std::cout << "[LOAD] Strict GPU model: " << model_utf8 << '\n';
  auto created = litertlm::LiteRtLm::Create(create_options, get_api);
  if (!created) {
    LogStatus(log, "create_error", created.status());
    std::cerr << "Create failed: " << created.status().message() << '\n';
    return 1;
  }
  litertlm::LiteRtLm context_a = created.TakeValue();
  CheckState checks;

  const auto prepared = context_a.Prepare();
  LogStatus(log, "prepare", prepared);
  Check(log, checks, "prepare_succeeds", prepared.ok(), prepared.message());

  auto info = context_a.GetModelInfo();
  if (!info) {
    LogStatus(log, "engine_info_error", info.status());
    Check(log, checks, "engine_info_available", false, info.status().message());
  } else {
    log.Event(
        "engine_info",
        "\"configured_backend\":" +
            std::to_string(static_cast<int>(info->configured_backend)) +
            ",\"resolved_backend\":" +
            std::to_string(static_cast<int>(info->resolved_backend)) +
            ",\"sampler_backend\":" +
            std::to_string(static_cast<int>(info->configured_sampler_backend)) +
            ",\"backend_evidence\":" +
            std::to_string(static_cast<int>(info->backend_evidence)) +
            ",\"max_context_tokens\":" +
            std::to_string(info->max_context_tokens) +
            ",\"description\":" + Q(info->backend_description));
    Check(log, checks, "strict_gpu_backend",
          info->resolved_backend == litertlm::Backend::Gpu &&
              info->configured_sampler_backend == litertlm::Backend::Gpu &&
              info->backend_evidence ==
                  LITERT_LM_BACKEND_EVIDENCE_FULLY_ACCELERATED,
          info->backend_description);
  }

  litertlm::ContextOptions b_options;
  b_options.system_prompt =
      "你是会话B的底层API记忆诊断助手。只使用会话B自己的历史。";
  auto b_created = context_a.NewContext(b_options);
  if (!b_created) {
    LogStatus(log, "context_b_create_error", b_created.status());
    return 1;
  }
  litertlm::LiteRtLm context_b = b_created.TakeValue();

  constexpr std::string_view kAlpha = "ALPHA_MEMORY_7319";
  constexpr std::string_view kBeta = "BETA_MEMORY_2846";

  auto a_store = Ask(context_a, log, "A_store",
                     "请记住这个仅用于本次测试的精确代码：ALPHA_MEMORY_7319。只回复已记住。", 96);
  auto b_store = Ask(context_b, log, "B_store",
                     "请记住这个仅用于本次测试的精确代码：BETA_MEMORY_2846。只回复已记住。", 96);
  auto a_recall = Ask(context_a, log, "A_recall_after_switch",
                      "上一轮我让你记住的精确代码是什么？请只复述代码本身。", 96);
  auto b_recall = Ask(context_b, log, "B_recall_after_switch",
                      "上一轮我让你记住的精确代码是什么？请只复述代码本身。", 96);

  const std::string memory_a = context_a.ExportMemoryJson();
  const std::string memory_b = context_b.ExportMemoryJson();
  Check(log, checks, "context_a_memory_contains_alpha",
        Contains(memory_a, kAlpha));
  Check(log, checks, "context_a_memory_excludes_beta",
        !Contains(memory_a, kBeta));
  Check(log, checks, "context_b_memory_contains_beta",
        Contains(memory_b, kBeta));
  Check(log, checks, "context_b_memory_excludes_alpha",
        !Contains(memory_b, kAlpha));
  Check(log, checks, "context_a_model_recalls_alpha",
        a_recall && Contains(a_recall->text, kAlpha),
        a_recall ? a_recall->text : "no response");
  Check(log, checks, "context_b_model_recalls_beta",
        b_recall && Contains(b_recall->text, kBeta),
        b_recall ? b_recall->text : "no response");

  auto restored_created = context_a.NewContext();
  if (!restored_created) {
    LogStatus(log, "restored_context_create_error", restored_created.status());
    return 1;
  }
  litertlm::LiteRtLm restored_context = restored_created.TakeValue();
  const auto imported = restored_context.ImportMemoryJson(memory_a);
  Check(log, checks, "memory_import_succeeds", imported.ok(),
        imported.message());
  auto restored_recall = Ask(restored_context, log, "A_recall_after_export_import",
                             "请再次只复述你记住的精确代码。", 96);
  Check(log, checks, "imported_context_recalls_alpha",
        restored_recall && Contains(restored_recall->text, kAlpha),
        restored_recall ? restored_recall->text : "no response");

  const std::string tools_json = R"JSON([
    {"type":"function","function":{"name":"select_probe_target","description":"提交一次诊断目标选择","parameters":{"type":"object","properties":{"seat":{"type":"integer"},"reason":{"type":"string"}},"required":["seat","reason"],"additionalProperties":false}}},
    {"type":"function","function":{"name":"submit_probe_speech","description":"提交一段自由生成的诊断发言","parameters":{"type":"object","properties":{"speech":{"type":"string"}},"required":["speech"],"additionalProperties":false}}}
  ])JSON";
  litertlm::ContextOptions tool_options;
  tool_options.system_prompt =
      "你是原生工具调用诊断助手。用户指定工具时必须调用该工具一次，不要用普通文本伪装工具调用。收到role=tool的执行结果后，用一句自然语言确认结果，不要再次调用工具。";
  tool_options.tools_json = tools_json;
  auto tool_created = context_a.NewContext(tool_options);
  if (!tool_created) {
    LogStatus(log, "tool_context_create_error", tool_created.status());
    return 1;
  }
  litertlm::LiteRtLm tool_context = tool_created.TakeValue();

  auto vote = Ask(tool_context, log, "native_tool_vote",
                  "现在执行诊断选择。合法席位是2、5、7，请调用select_probe_target并选择7号；reason由你自己用一句中文说明。", 192);
  const bool vote_has_tool =
      vote && Contains(vote->response_json, "select_probe_target") &&
      Contains(vote->response_json, "seat") &&
      Contains(vote->response_json, "7");
  Check(log, checks, "native_vote_returns_structured_tool_call", vote_has_tool,
        vote ? vote->response_json : "no response");
  Check(log, checks, "native_vote_finish_reason_is_tool_call",
        vote && vote->finish_reason == LITERT_LM_FINISH_TOOL_CALL,
        vote ? std::to_string(static_cast<int>(vote->finish_reason))
             : "no response");

  const std::string tool_result_json =
      R"JSON({"role":"tool","content":{"name":"select_probe_target","response":{"accepted":true,"seat":7}}})JSON";
  auto tool_summary = AskJson(tool_context, log, "native_tool_result_roundtrip",
                              tool_result_json, 128);
  Check(log, checks, "tool_result_roundtrip_generates_answer",
        tool_summary && !tool_summary->text.empty(),
        tool_summary ? tool_summary->text : "no response");

  auto speech = Ask(tool_context, log, "native_tool_free_speech",
                    "请调用submit_probe_speech发表一句你自由决定的中文发言，但内容必须自然地包含“月亮”两个字。", 192);
  const bool speech_has_tool =
      speech && Contains(speech->response_json, "submit_probe_speech") &&
      Contains(speech->response_json, "speech") &&
      Contains(speech->response_json, "月亮");
  Check(log, checks, "native_speech_returns_structured_tool_call",
        speech_has_tool, speech ? speech->response_json : "no response");

  log.Event("final_memory",
            "\"context_a\":" + Q(memory_a) +
                ",\"context_b\":" + Q(memory_b) +
                ",\"tool_context\":" + Q(tool_context.ExportMemoryJson()));

  const auto tool_close = tool_context.Close();
  const auto restored_close = restored_context.Close();
  const auto b_close = context_b.Close();
  const auto a_close = context_a.Close();
  Check(log, checks, "contexts_close_cleanly",
        tool_close.ok() && restored_close.ok() && b_close.ok() &&
            a_close.ok());

  log.Event("probe_finish", "\"passed\":" + std::to_string(checks.passed) +
                                ",\"failed\":" + std::to_string(checks.failed) +
                                ",\"pass\":" +
                                std::string(checks.failed == 0 ? "true" : "false"));

  FreeLibrary(wrapper);
  for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
    FreeLibrary(*it);
  }

  std::cout << "\nProbe complete: " << checks.passed << " passed, "
            << checks.failed << " failed.\nLog: "
            << ToUtf8(log_path.wstring()) << '\n';
  return checks.failed == 0 ? 0 : 1;
}
