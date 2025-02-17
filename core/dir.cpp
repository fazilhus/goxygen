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

		std::cout << "[INFO] Indexing all files in directory\n";
		for (auto dir_entry = std::filesystem::recursive_directory_iterator(path_); dir_entry != std::filesystem::recursive_directory_iterator(); ++dir_entry) {
			if (util::is_dir(*dir_entry) && util::is_dir_blacklisted(dir_entry->path().filename().wstring(), ignored_folders_)) {
				dir_entry.disable_recursion_pending();
				continue;
			}

			if (util::is_file(*dir_entry)) {
				if (dir_entry->path().extension() == ".gd"/* || dir_entry->path().extension() == ".cs"*/) {
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
		std::cout << "[INFO] Finished indexing all files in directory\n";

		std::cout << "[INFO] Parsing scene files\n";
		for (auto& val : scene_files) {
			dott_parser p{ val };
			if (!p.parse_scene_header())
				return;
			file_tree_[val->get_uid()] = val;
		}

		for (auto& val : scene_files) {
			dott_parser p{ val };
			p.set_root_path(path_);
			if (!p.parse_scene_file_contents(file_tree_, script_files_, resource_files_))
				return;
		}
		std::cout << "[INFO] Finished parsing scene files\n";

		std::cout << "[INFO] Parsing resource files\n";
		for (auto& val : resource_files) {
			dott_parser p{ val };
			if (!p.parse_resource_header())
				return;
			resource_files_[val->get_uid()] = val;
		}

		for (auto& val : resource_files) {
			dott_parser p{ val };
			p.set_root_path(path_);
			if (!p.parse_resource_file_contents(file_tree_, script_files_, resource_files_))
				return;
		}
		std::cout << "[INFO] Finished parsing resource files\n";

		std::cout << "[INFO] Parsing script files\n";
		for (auto& [_, val] : script_files_) {
			script_parser p{ val };
			p.parse();
		}
		std::cout << "[INFO] Finished parsing script files\n";
	}

	void dir::gen_docs() {
		auto docs_dir = path_ / "docs";
		if (std::filesystem::exists(docs_dir)) {
			std::filesystem::remove_all(docs_dir);
		}

		std::filesystem::create_directory(docs_dir);

		std::cout << "[INFO] Writing scene files\n";
		for (const auto& [_, file] : file_tree_) {
			auto doc_path = file->get_path();
			doc_path = std::filesystem::relative(doc_path, path_);
			doc_path = docs_dir / doc_path;
			std::filesystem::create_directories(doc_path.parent_path());
			doc_path.replace_filename(doc_path.filename().wstring() + L".md");
			std::wofstream out{ doc_path, std::ios::out | std::ios::binary };

			out.write(L"#scene\n", 7);

			out.write(L"# Node Tree\n", 12);
			auto& nodes = file->get_node_tree();
			for (auto it = nodes.begin(); it != nodes.end(); ++it) {
				for (std::size_t i = 0; i < (*it)->depth - 1; ++i) {
					out.put('\t');
				}
				out.write(L"- ", 2);
				out.write((*it)->name.data(), (*it)->name.size());
				out.put('\n');
				
				for (const auto& [f, s] : (*it)->ext_resource_fields) {
					if (!s.expired()) {
						for (std::size_t i = 0; i < (*it)->depth; ++i) {
							out.put('\t');
						}
						out.write(L"  *", 3);
						out.write(f.c_str(), f.size());
						out.write(L"*: ", 3);
						write_named_file_link(out, docs_dir, s.lock()->get_path());
						out.put('\n');
					}
				}
			}

			out.write(L"# External Resources\n", 21);
			out.write(L"## Scenes\n", 10);
			for (const auto& [_, child] : file->get_packed_scenes()) {
				out.write(L"- ", 2);
				write_named_file_link(out, docs_dir, child->get_path());
				out.put('\n');
			}

			out.write(L"## Scripts\n", 11);
			for (const auto& [_, script] : file->get_scripts()) {
				out.write(L"- ", 2);
				write_named_file_link(out, docs_dir, script->get_path());
				out.put('\n');
			}
			
			out.write(L"## Resources\n", 13);
			for (const auto& [_, resource] : file->get_ext_resources()) {
				out.write(L"- ", 2);
				write_named_file_link(out, docs_dir, resource->get_path());
				out.put('\n');
			}
			for (const auto& [_, resource] : file->get_ext_resource_other()) {
				out.write(L"- ", 2);
				out.write(resource.name.data(), resource.name.size());
				out.write(L": ", 2);
				out.write(resource.type.data(), resource.type.size());
				out.put('\n');
			}

			out.close();
		}

		std::cout << "[INFO] Writing resource files\n";
		for (const auto& [_, file] : resource_files_) {
			auto doc_path = file->get_path();
			doc_path = std::filesystem::relative(doc_path, path_);
			doc_path = docs_dir / doc_path;
			std::filesystem::create_directories(doc_path.parent_path());
			doc_path.replace_filename(doc_path.filename().wstring() + L".md");
			std::wofstream out{ doc_path, std::ios::out | std::ios::binary };

			out.write(L"#resource\n", 10);
			write_tres_resource(out, file, docs_dir);

			out.write(L"# External Resources\n", 21);
			out.write(L"## Scripts\n", 11);
			for (const auto& [_, script] : file->get_scripts()) {
				out.write(L"- ", 2);
				write_named_file_link(out, docs_dir, script->get_path());
				out.put('\n');
			}
			
			out.write(L"## Scenes\n", 10);
			for (const auto& [_, child] : file->get_packed_scenes()) {
				out.write(L"- ", 2);
				write_named_file_link(out, docs_dir, child->get_path());
				out.put('\n');
			}
			
			out.write(L"## Resources\n", 13);
			for (const auto& [_, resource] : file->get_ext_resources()) {
				out.write(L"- ", 2);
				write_named_file_link(out, docs_dir, resource->get_path());
				out.put('\n');
			}
			for (const auto& [_, resource] : file->get_ext_resource_other()) {
				out.write(L"- ", 2);
				out.write(resource.name.data(), resource.name.size());
				out.write(L": ", 2);
				out.write(resource.type.data(), resource.type.size());
				out.put('\n');
			}
			
			out.close();
		}

		std::cout << "[INFO] Writing script files\n";
		for (const auto& [_, file] : script_files_) {
			auto doc_path = file->get_path();
			doc_path = std::filesystem::relative(doc_path, path_);
			doc_path = docs_dir / doc_path;
			std::filesystem::create_directories(doc_path.parent_path());
			doc_path.replace_filename(doc_path.filename().string() + ".md");
			std::wofstream out{ doc_path, std::ios::out | std::ios::binary };
			
			const auto& sc = file->get_script_class();

			out.write(L"#script", 7);
			for (const auto& tag : sc.tags) {
				out.write(L" #", 2);
				out.write(tag.data(), tag.size());
			}
			out.put('\n');
			
			out.write(L"## Extends ", 11);
			out.write(sc.parent.data(), sc.parent.size());
			out.put('\n');

			out.write(L"## Class ", 9);
			out.write(sc.name.data(), sc.name.size());
			out.put('\n');

			if (!sc.short_desc.empty()) {
				out.put('\t');
				out.write(sc.short_desc.data(), sc.short_desc.size());
				out.put('\n');
			}

			out.write(L"## Variables\n", 13);
			for (const auto& cat : sc.categories) {
				if (!cat.name.empty()) {
					out.write(L"- ", 2);
					out.write(L"### ", 4);
					out.write(cat.name.data(), cat.name.size());
					out.put('\n');
				}
				else {
					out.write(L"- ", 2);
					out.write(L"### Default Export Group\n", 25);
				}
				
				for (const auto& var : cat.variables) {
					out.put('\t');
					out.write(L"- ", 2);
					if (var.name[0] == '_') {
						out.put('\\');
					}
					out.write(var.name.data(), var.name.size());
					out.write(L" : ", 3);
					out.write(var.type.data(), var.type.size());
					out.put('\n');

					if (!var.short_desc.empty()) {
						out.write(L"\t\t", 2);
						out.write(var.short_desc.data(), var.short_desc.size());
						out.put('\n');
					}
				}
			}

			out.write(L"## Functions\n", 13);
			for (const auto& func : sc.functions) {
				out.write(L"- ", 2);
				if (func.name[0] == '_') {
					out.put('\\');
				}
				out.write(func.name.data(), func.name.size());
				out.put('\n');

				if (!func.short_desc.empty()) {
					out.put('\t');
					out.write(func.short_desc.data(), func.short_desc.size());
					out.put('\n');
				}
				
				out.write(L"\tArguments\n", 11);
				for (const auto& arg : func.arguments) {
					out.write(L"\t- ", 3);
					if (arg.name[0] == '_') {
						out.put('\\');
					}
					out.write(arg.name.data(), arg.name.size());
					out.write(L" : ", 3);
					out.write(arg.type.data(), arg.type.size());
					out.put('\n');
				}
				out.write(L"\tReturn type: ", 14);
				out.write(func.return_type.data(), func.return_type.size());
				out.put('\n');
			}
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

	void dir::write_named_file_link(std::wofstream& out, const std::filesystem::path& docs_path,
		const std::filesystem::path& file_path) const {
		auto doc_path = std::filesystem::relative(file_path, path_);
		doc_path = docs_path / doc_path;
		doc_path.replace_filename(doc_path.filename().wstring() + L".md");
		const auto file_name = doc_path.filename().wstring();
		const auto link_name = doc_path.stem().wstring();
		out.put('[');
		out.write(link_name.c_str(), link_name.size());
		out.write(L"](", 2);
		out.write(file_name.c_str(), file_name.size());
		out.put(')');
	}

	void dir::write_tres_resource(std::wofstream& out, const std::weak_ptr<resource_file>& file, const std::filesystem::path& docs_path) const {
		if (file.expired()) return;
		const auto& f = file.lock();
		
		out.write(L"# Using\n", 8);
		write_tres_resource_(out, docs_path, f->get_resource());

		out.write(L"## Sub_Resources\n", 17);
		for (const auto& [_, res] : f->get_sub_resources()) {
			write_tres_resource_(out, docs_path, *res.get(), true);
		}
	}

	void dir::write_tres_resource_(std::wofstream& out, const std::filesystem::path& docs_path,
		const resource_file::resource& res, bool sub_res) const {
		if (sub_res) {
			out.write(res.type.data(), res.type.size());
			out.put('\n');
		}
		for (const auto& [name, ext_res] : res.res_file_fields) {
			if (sub_res) {
				out.write(L"\t- ", 3);
			}
			else {
				out.write(L"- ", 2);
			}
			out.write(name.data(), name.size());
			out.write(L": ", 2);

			if (ext_res.expired()) {
				out.write(L"Unknown file\n", 13);
			}
			else {
				write_named_file_link(out, docs_path, ext_res.lock()->get_path());
				out.put('\n');
			}
		}
		
		for (const auto& [name, ext_other_res] : res.res_other_fields) {
			if (sub_res) {
				out.write(L"\t- ", 3);
			}
			else {
				out.write(L"- ", 2);
			}
			out.write(name.data(), name.size());
			out.write(L": ", 2);
			out.write(ext_other_res.data(), ext_other_res.size());
			out.put('\n');
		}

		for (const auto& [name, ext_res] : res.sub_res_fields) {
			if (sub_res) {
				out.write(L"\t- ", 3);
			}
			else {
				out.write(L"- ", 2);
			}
			out.write(name.data(), name.size());
			out.write(L": ", 2);

			if (ext_res.expired()) {
				out.write(L"Unknown sub_resource\n", 13);
			}
			else {
				const auto& r = ext_res.lock();
				out.write(r->type.data(), r->type.size());
				out.put('\n');
			}
		}

		for (const auto& [name, val] : res.fields) {
			if (sub_res) {
				out.write(L"\t- ", 3);
			}
			else {
				out.write(L"- ", 2);
			}
			out.write(name.data(), name.size());
			out.write(L": ", 2);
			out.write(val.data(), val.size());
			out.put('\n');
		}
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
