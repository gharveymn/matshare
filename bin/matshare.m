classdef matshare
	
	% This is stored information
	properties (Hidden)
		shared_data;
	end
	
	% This is the property that users will query
	properties (Dependent)
		data
	end
	
	methods
		
		function obj = matshare(num_objs)
			if(nargin > 0 && num_objs > 0)
				obj(num_objs) = matshare;
			end
		end
		
		function obj = set.data(obj, in)
			obj.shared_data = matshare_(0, in);
			matshare_(12);
		end
		
		function ret = get.data(obj)
			ret = obj.shared_data;
		end
		
		function overwrite(obj, in)
			matshare_(8, obj.shared_data, in);
		end
		
		function safeoverwrite(obj, in)
			matshare_(9, obj.shared_data, in);
		end
		
		function cleardata(obj)
			matshare_(6, obj.data);
		end
		
		function ret = makelocalcopy(obj)
			ret = matshare_(4, obj.data);
		end
				
	end
	
	methods (Static)
		
		% these are just the entry functions but in static method form
		
		function newobj = share(in)
			newobj = matshare;
			newobj.data = in;
		end
		
		function [newestobj, newvarobjs, allvarobjs] = fetch
			if(nargout <= 1)
				newestobj = matshare;
				newestobj.shared_data = matshare_(1);
			elseif(nargout == 3)
				[newestvar, newvars, allvars] = matshare_(1);
				newestobj = matshare;
				
				if(numel(newvars) > 0)
					newvarobjs = num2cell(matshare(numel(newvars)));
				else
					newvarobjs = [];
				end
				
				if(numel(allvars) > 0)
					allvarobjs = num2cell(matshare(numel(allvars)));
				else
					allvarobjs = [];
				end
				
				newestobj.shared_data = newestvar;
				
				for i = 1:numel(newvars)
					newvarobjs{i}.shared_data = newvars{i};
				end
				
				for i = 1:numel(allvars)
					allvarobjs{i}.shared_data = allvars{i};
				end
			else
				[newestvar, newvars] = matshare_(1);
				
				newestobj = matshare;
				
				if(numel(newvars) > 0)
					newvarobjs = num2cell(matshare(numel(newvars)));
				end
				
				newestobj.shared_data = newestvar;
				
				for i = 1:numel(newvars)
					newvarobjs{i}.shared_data = newvars{i};
				end
			end
			matshare_(12);
		end
		
		function detach
			matshare_(2);
		end
		
		function config(varargin)
		    matshare_(3,varargin{:});
		end
		
		function ret = localcopy(in)
			ret = matshare_(4, in);
		end
		
		function debug
			matshare_(5)
		end
		
		function clear(varargin)
			matshare_(6, varargin{:});
		end
		
		function reset
			matshare_(7);
		end
		
		function lock
			matshare_(10);
		end
		
		function unlock
			matshare_(11);
		end
		
	end
end

