function sourcesfromdirs(root, dirs)
	local res = {}
	for i,v in dirs do
		local prefix
		if v == "" then prefix = root..v else prefix = root..v.."/" end
		local files = matchfiles(
			prefix.."*.cpp",
			prefix.."*.h",
			prefix.."*.inl",
			prefix.."*.asm")
		listconcat(res, files)
	end
	return res
end

function trimrootdir(root, dirs)
	for i,v in dirs do
		dirs[i] = strsub(v, strlen(root))
	end
end

function listconcat(list, values)
	for i,v in values do
		table.insert(list, v)
	end
end
