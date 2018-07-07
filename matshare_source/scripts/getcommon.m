dblmeta = ?double;
dblmethods = dblmeta.MethodList;
dblmnames = cell(size(dblmethods));
for i = 1:numel(dblmnames)
	dblmnames{i} = dblmethods(i).Name;
end

sglmeta = ?single;
sglmethods = sglmeta.MethodList;
sglmnames = cell(size(sglmethods));
for i = 1:numel(sglmnames)
	sglmnames{i} = sglmethods(i).Name;
end

common = intersect(dblmnames, sglmnames);
diff = setdiff(dblmnames, sglmnames);

c1 = 1;
c2 = 1;
clear inputs outputs
for i = 1:numel(dblmnames)
	if(~strcmp(dblmethods(i).InputNames{1}, 'varargin'))
		inputs{c1} = dblmethods(i).InputNames{1};
		c1 = c1 + 1;
	end
	if(~strcmp(dblmethods(i).OutputNames{1}, 'varargout'))
		outputs{i} = dblmethods(i).OutputNames{1};
		c2 = c2 + 1;
	end
end