clean-coverage:
	@rm -fr .coverage coverage

coverage: test clean-coverage
	@echo "Generating $@"
	@$(LCOV) --directory . --capture --base-directory=. --output-file .coverage
	@$(GENHTML) --legend --output-directory coverage/ --title "pecl/apc code coverage" .coverage

