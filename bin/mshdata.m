classdef mshdata < handle
	
	properties (AbortSet, Access=private)
		deepdata
	end
	
	properties (Dependent, GetObservable, SetObservable)
		data
	end
	
	properties (Constant, Access=private, Hidden)
		INIT = uint8(0);
		CLONE = uint8(1);
		ATTACH = uint8(2);
		DETACH = uint8(3);
		FREE = uint8(4);
		FETCH = uint8(5);
		COMPARE = uint8(6);
	end
	
	properties (Access=private, Hidden)
		internal_change = false;
	end
	
	methods
		function obj = mshdata
			obj.deepdata = matshare_(obj.INIT);
			addlistener(obj, 'data', 'PreSet', @(src,evnt)mshdata.preSetCallback(obj,src,evnt));	
			addlistener(obj, 'data', 'PreGet', @(src,evnt)mshdata.preGetCallback(obj,src,evnt));	
		end
		
		function set.data(obj,in)
			obj.deepdata = matshare_(obj.CLONE,in);
			matshare_(obj.ATTACH);
		end
		
		function ret = get.data(obj)
			ret = obj.deepdata;
		end
		
		function delete(obj)
			obj.deepdata = matshare_(obj.DETACH);
			matshare_(obj.FREE);
			clear obj.deepdata;
		end
	end
	
	methods (Static)
		function preSetCallback(obj,~,~)
			obj.deepdata = matshare_(obj.DETACH);
			matshare_(obj.FREE);
		end
		
		function preGetCallback(obj,~,~)
			if(~matshare_(obj.COMPARE, obj.deepdata))
				obj.deepdata = matshare_(obj.DETACH);
				obj.deepdata = matshare_(obj.FETCH);
				matshare_(obj.ATTACH);
			end
		end
	end
end

