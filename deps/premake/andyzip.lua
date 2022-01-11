andyzip = {
	source = path.join(dependencies.basePath, "andyzip"),
}

function andyzip.import()
	andyzip.includes()
end

function andyzip.includes()
	includedirs {
		path.join(andyzip.source, "include"),
	}
end

function andyzip.project()

end

table.insert(dependencies, andyzip)
