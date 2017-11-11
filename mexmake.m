addpath('src')
output_path = [pwd '/bin'];
cd src

try
	
	sources = {'matshare_.c',...
                'utils.c'};
	
	mexflags = {'-g', '-v', 'CFLAGS="$CFLAGS -std=c99"', '-outdir', output_path};
	
	if(strcmp(mex.getCompilerConfigurations('C','Selected').ShortName, 'mingw64'))
		
		sources = [sources,...
			{'extlib/mman-win32/mman.c'}
			];
		
		mex(mexflags{:}, sources{:})
		
	elseif(strncmpi(mex.getCompilerConfigurations('C','Selected').ShortName, 'MSVC',4))
		
		sources = [sources,...
			{'extlib/mman-win32/mman.c'}
			];
		
		mex(mexflags{:}, sources{:})
		
	elseif(strcmp(mex.getCompilerConfigurations('C','Selected').ShortName, 'gcc'))
		
		mex(mexflags{:}, sources{:})
		
	end
	
	cd ..
	
catch ME
	
	cd ..
	rethrow(ME)
	
end