function matshare(matshare_operation, variable)
	%MATSHARE Entry function for matshare. Passes the variable name if necessary.
	
	varname = inputname(2);
	
	if(strcmp(matshare_operation, 'share'))
		matshare_(int32(0), variable);
	elseif(strcmp(matshare_operation, 'get'))
		shared_variable = matshare_(int32(1), uint8(varname));
		assignin('caller', varname, shared_variable);
	elseif(strcmp(matshare_operation, 'unshare'))
		matshare_(int32(2), uint8(varname));
	else
		no_such_operation_exception = MException('matshare:noSuchOperation', ...
			['The specified operation is invalid.' ...
			 'Available operations are ''share'', ''get'', and ''unshare''.']);
		throw(no_such_operation_exception)
	end
		
end

