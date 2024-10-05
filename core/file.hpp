#ifndef DOCS_GEN_FILE_H
#define DOCS_GEN_FILE_H

#include <filesystem>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace docs_gen_core {

	class file {
	protected:
		std::filesystem::path path_;
		std::wstring title_;

		file() = default;
		explicit file(const std::filesystem::path& path);
		file(const file& other);
		file(file&& other) noexcept;
		virtual ~file() = default;
	
	public:
		file& operator=(const file&) = delete;
		file& operator=(file&&) = delete;
		
		[[nodiscard]] const std::filesystem::path& get_path() const { return path_; }
		[[nodiscard]] const std::wstring& get_title() const { return title_; }
	};

	struct code_snippet {
		std::streampos comment_start_pos;
		std::streampos comment_length;
		std::streampos code_start_pos;
		std::streampos code_length;
	};

	class script_file final : public file {
		std::vector<code_snippet> code_snippets_;
		
	public:
		script_file() = default;
		explicit script_file(const std::filesystem::path& path);
		script_file(const script_file& other);
		script_file(script_file&& other) noexcept;
		~script_file() override = default;

		script_file& operator=(const script_file& other);
		script_file& operator=(script_file&& other) noexcept;

		void push_code_snippet(const code_snippet& snippet) { code_snippets_.push_back(snippet); }
		[[nodiscard]] const std::vector<code_snippet>& get_code_snippets() const { return code_snippets_; }
	};

	class resource_file final : public file {
		std::wstring uid_;
		std::wstring script_class_;
		std::shared_ptr<script_file> script_;

	public:
		resource_file() = default;
		explicit resource_file(const std::filesystem::path& path);
		resource_file(const resource_file& other);
		resource_file(resource_file&& other) noexcept;
		~resource_file() override = default;

		resource_file& operator=(const resource_file& other);
		resource_file& operator=(resource_file&& other) noexcept;

		void set_uid(const std::wstring& s) { uid_ = s; }
		void set_script_class(const std::wstring& s) { script_class_ = s; }
		void set_script(const std::shared_ptr<script_file>& script) { script_ = script; }

		[[nodiscard]] const std::wstring& get_uid() const { return uid_; }
		[[nodiscard]] const std::wstring& get_script_class() const { return script_class_; }
		[[nodiscard]] const std::shared_ptr<script_file>& get_script() const { return script_; }
		[[nodiscard]] std::shared_ptr<script_file>& get_script() { return script_; }
	};

	class scene_file final : public file {
		std::wstring uid_;
		std::unordered_map<std::wstring, std::shared_ptr<scene_file>> packed_scenes_;
		std::unordered_map<std::wstring, std::shared_ptr<script_file>> scripts_;
		std::unordered_map<std::wstring, std::wstring> sub_resources_;

	public:
		scene_file() = default;
		explicit scene_file(const std::filesystem::path& path);
		scene_file(const scene_file& other);
		scene_file(scene_file&& other) noexcept;
		~scene_file() override = default;

		scene_file& operator=(const scene_file& other);
		scene_file& operator=(scene_file&& other) noexcept;

		void set_uid(const std::wstring& s) { uid_ = s; }
		void push_packed_scene(const std::wstring& key, const std::shared_ptr<scene_file>& child);
		void push_script(const std::wstring& key, const std::shared_ptr<script_file>& script);
		void push_sub_resource(const std::wstring& key, const std::wstring& sub_resource);

		[[nodiscard]] const std::wstring& get_uid() const { return uid_; }
		[[nodiscard]] const std::unordered_map<std::wstring, std::shared_ptr<scene_file>>& get_packed_scenes() const { return packed_scenes_; }
		[[nodiscard]] std::unordered_map<std::wstring, std::shared_ptr<scene_file>>& get_packed_scenes() { return packed_scenes_; }
		[[nodiscard]] const std::unordered_map<std::wstring, std::shared_ptr<script_file>>& get_scripts() const { return scripts_; }
		[[nodiscard]] std::unordered_map<std::wstring, std::shared_ptr<script_file>>& get_scripts() { return scripts_; }
		[[nodiscard]] const std::unordered_map<std::wstring, std::wstring>& get_sub_resources() const { return sub_resources_; }
		[[nodiscard]] std::unordered_map<std::wstring, std::wstring>& get_sub_resources() { return sub_resources_; }
	};

	struct scene_file_hash {
		bool operator()(const std::shared_ptr<scene_file>& f) const noexcept {
			return std::hash<std::wstring>{}(f->get_uid());
		}
	};

} // docs_gen_core

#endif // DOCS_GEN_FILE_H
