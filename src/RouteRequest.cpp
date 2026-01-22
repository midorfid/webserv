
void routePath(const std::string &req_path, ) {

	if (req_path == "/") {

		std::string index;

		if (!location->getIndex(index))

			return resolveErrorAction(404, serv_cfg);

		struct stat st;

		std::string physAndIdxPath = phys_path + index;

		if (stat(physAndIdxPath.c_str(), &st) == 0) {

			return resolveFileAction(physAndIdxPath, &st);

		} // handle if index not found, autoindex off TODO as well as multiple indexes

		return resolveErrorAction(404, serv_cfg);

	}

	if (normalizePath(phys_path) == false) {

		return resolveErrorAction(404, serv_cfg);

	}

	return checkReqPath(phys_path, serv_cfg, location);

}
