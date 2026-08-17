spec-analysis:	
	set -a; \
	. .env; \
	set +a; \
	tools/submit_ekom_analysis.sh

spec-implementation:
	set -a; \
	. .env; \
	set +a; \
	tools/submit_ekom_implementation.sh
