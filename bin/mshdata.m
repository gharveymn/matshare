classdef mshdata < handle
	
	properties (AbortSet, GetObservable, SetObservable)
		data
	end
	
	properties (Constant, Access=private, Hidden)
		msh_INIT = uint8(0);
		msh_CLONE = uint8(1);
		msh_ATTACH = uint8(2);
		msh_DETACH = uint8(3);
		msh_FREE = uint8(4);
	end
	
	methods
		function obj = mshdata
			obj.data = matshare_(obj.msh_INIT);
			addlistener(obj, 'data', 'PreSet', @(src,evnt)mshdata.propChangeCallback(obj,src,evnt));	
			addlistener(obj, 'data', 'PreGet', @(src,evnt)mshdata.propChangeCallback(obj,src,evnt));	
		end
		
		function set.data(obj,in)
			obj.data = matshare_(obj.msh_CLONE,in);
			matshare_(obj.msh_ATTACH);
		end
		
		function delete(obj,in)
			obj.data = matshare_(obj.msh_DETACH);
			matshare_(obj.msh_FREE);
			clear obj.data;
		end
	end
	
	methods (Static)
		function propChangeCallback(obj,src,evnt)
			obj.data = matshare_(obj.msh_DETACH);
			matshare_(obj.msh_FREE);
		end
	end
end

