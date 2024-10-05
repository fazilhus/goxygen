#include "dir.hpp"

#include <iostream>

#include "parser.hpp"

namespace docs_gen_core {
	bool dir::set_path(const std::wstring& path) {
		const auto temp = std::filesystem::path(path);

		if (!util::is_valid_path(temp)) {
			return false;
		}

		path_ = temp;
		return true;
	}

	void dir::construct_file_tree() {
		std::vector<std::shared_ptr<scene_file>> scene_files;
		std::vector<std::shared_ptr<resource_file>> resource_files;
		for (auto dir_entry = std::filesystem::recursive_directory_iterator(path_); dir_entry != std::filesystem::recursive_directory_iterator(); ++dir_entry) {
			if (util::is_dir(*dir_entry) && util::is_dir_blacklisted(dir_entry->path().filename().wstring(), ignored_folders_)) {
				dir_entry.disable_recursion_pending();
				continue;
			}

			if (util::is_file(*dir_entry)) {
				if (dir_entry->path().extension() == ".gd" || dir_entry->path().extension() == ".cs") {
					auto f = script_file(dir_entry->path());
					script_files_[dir_entry->path().relative_path().wstring()] = std::make_shared<script_file>(f);
				}

				if (dir_entry->path().extension() == ".tscn") {
					auto f = scene_file(dir_entry->path());
					scene_files.push_back(std::make_shared<scene_file>(f));
				}

				if (dir_entry->path().extension() == ".tres") {
					auto f = resource_file(dir_entry->path());
					resource_files.push_back(std::make_shared<resource_file>(f));
				}
			}
		}

		for (auto& val : scene_files) {
			parser p{ val };
			if (!p.parse_scene_header())
				return;
			file_tree_[val->get_uid()] = val;
		}

		for (auto& val : scene_files) {
			parser p{ val };
			p.set_root_path(path_);
			if (!p.parse_scene_ext_resources(file_tree_, script_files_, resource_files_))
				return;
		}
	}

	void dir::gen_docs() {
		auto docs_dir = path_ / "docs";
		if (std::filesystem::exists(docs_dir)) {
			std::filesystem::remove_all(docs_dir);
		}

		std::filesystem::create_directory(docs_dir);
		
		for (const auto& [_, file] : file_tree_) {
			auto doc_path = file->get_path();
			doc_path = std::filesystem::relative(doc_path, path_);
			doc_path = docs_dir / doc_path;
			std::filesystem::create_directories(doc_path.parent_path());
			doc_path.replace_extension(".md");
			std::wofstream out{ doc_path, std::ios::out | std::ios::binary };

			out.write(L"#scene\n", 7);

			out.write(L"# External Resources\n", 21);
			out.write(L"## Scenes\n", 10);
			for (const auto& [_, child] : file->get_packed_scenes()) {
				auto child_doc_path = std::filesystem::relative(child->get_path(), path_);
				child_doc_path = docs_dir / child_doc_path;
				child_doc_path.replace_extension(".md");
				auto child_filename = child_doc_path.filename().wstring();
				auto child_stem = child_doc_path.stem().wstring();
				write_named_file_link(out, child_filename, child_stem);
			}

			out.write(L"## Scripts\n", 11);
			for (const auto& [_, child] : file->get_scripts()) {
				auto script_doc_path = std::filesystem::relative(child->get_path(), path_);
				script_doc_path = docs_dir / script_doc_path;
				script_doc_path.replace_filename(script_doc_path.filename().wstring() + L".md");
				auto script_filename = script_doc_path.filename().wstring();
				auto script_stem = script_doc_path.stem().wstring();
				write_named_file_link(out, script_filename, script_stem);
			}
			
			out.write(L"## Resources\n", 13);

			out.close();
		}

		for (const auto& [_, file] : script_files_) {
			auto doc_path = file->get_path();
			doc_path = std::filesystem::relative(doc_path, path_);
			doc_path = docs_dir / doc_path;
			std::filesystem::create_directories(doc_path.parent_path());
			doc_path.replace_filename(doc_path.filename().string() + ".md");
			std::ofstream out{ doc_path, std::ios::out | std::ios::binary };
			out.write("#script\n", 8);
			out.close();
		}

		// TODO develop a proper way of item coloring in obsidian
		auto obsidian_dir = docs_dir / ".obsidian";
		std::filesystem::create_directory(obsidian_dir);
		
		std::ofstream out{ obsidian_dir / "graph.json", std::ios::out | std::ios::binary };
		
		std::string temp =
			R"({"colorGroups":[{"query":"tag:#scene","color":{"a":1,"rgb":14048348}},{"query":"tag:#script","color":{"a":1,"rgb":6577366}},{"query":"tag:#resource","color":{"a":1,"rgb":4521728}}]})";
		out.write(temp.data(), temp.size());
		out.close();
	}

	bool dir::is_ignored(const std::filesystem::path& path) const {
		for (const auto& ignored : ignored_folders_) {
			if (path.compare(ignored) == 0) {
				std::cout << "\tshould be ignored\n";
				return true;
			}

			auto rel = std::filesystem::relative(path, ignored);
			std::cout << path << '\n';
			if (!rel.empty() && rel.native()[0] != '.') {
				std::cout << "\tshould be ignored\n";
				return true;
			}
		}

		return false;
	}

	void dir::write_named_file_link(std::wofstream& out, const std::wstring& file_name,
		const std::wstring& link_name) {
		out.write(L"[[", 2);
		out.write(file_name.c_str(), file_name.size());
		out.put('|');
		out.write(link_name.c_str(), link_name.size());
		out.write(L"]]\n", 3);
	}

	namespace util {

		bool is_valid_path(const std::filesystem::path& path) {
			return std::filesystem::exists(path);
		}

		bool is_file(const std::filesystem::path& path) {
			return std::filesystem::is_regular_file(path);
		}

		bool is_dir(const std::filesystem::path& path) {
			return std::filesystem::is_directory(path);
		}

		bool is_dir_blacklisted(const std::wstring& dir_name, const std::vector<std::wstring>& ignored) {
			for (const auto& ignored_name : ignored) {
				if (dir_name == ignored_name) {
					return true;
				}
			}
			return false;
		}
		
	} // util

} // docs_gen_core
