<?php

namespace Drush\Commands;

use Drupal\Component\Gettext\PoStreamWriter;
use Drupal\Core\Cache\CacheBackendInterface;
use Drupal\Core\Config\ConfigFactory;
use Drupal\Core\Entity\EntityTypeManagerInterface;
use Drupal\Core\Extension\ModuleHandlerInterface;
use Drupal\Core\Language\LanguageInterface;
use Drupal\Core\Locale\CountryManagerInterface;
use Drupal\language\ConfigurableLanguageManagerInterface;
use Drupal\language\Entity\ConfigurableLanguage;
use Drupal\locale\PoDatabaseReader;
use Drush\Utils\StringUtils;

/**
 * Implements the Drush language commands.
 */
class DrushLanguageCommands extends DrushCommands {

  /**
   * The cache.page bin.
   *
   * @var \Drupal\Core\Cache\CacheBackendInterface
   */
  protected $cachePage;

  /**
   * The config.factory service.
   *
   * @var \Drupal\Core\Config\ConfigFactory
   */
  protected $configFactory;

  /**
   * The country_manager service.
   *
   * @var \Drupal\Core\Locale\CountryManagerInterface
   */
  protected $countryManager;

  /**
   * The entity_type.manager service.
   *
   * @var \Drupal\Core\Entity\EntityTypeManagerInterface
   */
  protected $entityTypeManager;

  /**
   * The language_manager service.
   *
   * @var \Drupal\language\ConfigurableLanguageManagerInterface
   */
  protected $languageManager;

  /**
   * The module_handler service.
   *
   * @var \Drupal\Core\Extension\ModuleHandlerInterface
   */
  protected $moduleHandler;

  /**
   * DrushLanguageCommands constructor.
   *
   * @param \Drupal\Core\Cache\CacheBackendInterface $cachePage
   *   The cache.page bin.
   * @param \Drupal\Core\Config\ConfigFactory $configFactory
   *   The config.factory service.
   * @param \Drupal\Core\Locale\CountryManagerInterface $countryManager
   *   The country_manager service.
   * @param \Drupal\Core\Entity\EntityTypeManagerInterface $entityTypeManager
   *   The entity_type.manager service.
   * @param \Drupal\language\ConfigurableLanguageManagerInterface $languageManager
   *   The language_manager service.
   * @param \Drupal\Core\Extension\ModuleHandlerInterface $moduleHandler
   *   The module_handler service.
   */
  public function __construct(
    CacheBackendInterface $cachePage,
    ConfigFactory $configFactory,
    CountryManagerInterface $countryManager,
    EntityTypeManagerInterface $entityTypeManager,
    ConfigurableLanguageManagerInterface $languageManager,
    ModuleHandlerInterface $moduleHandler
  ) {
    $this->cachePage = $cachePage;
    $this->configFactory = $configFactory;
    $this->countryManager = $countryManager;
    $this->entityTypeManager = $entityTypeManager;
    $this->languageManager = $languageManager;
    $this->moduleHandler = $moduleHandler;
  }

  /**
   * Add and import one or more new language definitions.
   *
   * @param array $langcodes
   *   A comma-delimited list of langcodes for which a definition will be added.
   *
   * @command language:add
   *
   * @aliases langadd,language-add
   */
  public function add(array $langcodes) {
    $langcodes = StringUtils::csvToArray($langcodes);

    if (empty($langcodes)) {
      $this->logger()->error('Please provide one or more comma-separated language codes as arguments.');
      return;
    }

    foreach ($langcodes as $langcode) {
      $messageArgs = ['langcode' => $langcode];
      // In the foreach loop because the list changes on successful iterations.
      $languages = $this->languageManager->getLanguages();

      // Do not re-add existing languages.
      if (isset($languages[$langcode])) {
        $this->logger()->warning('The language with code {langcode} already exists.', $messageArgs);
        continue;
      }

      // Only allow adding languages for predefined langcodes.
      // In the foreach loop because the list changes on successful iterations.
      $predefined = $this->languageManager->getStandardLanguageListWithoutConfigured();
      if (!isset($predefined[$langcode])) {
        $this->logger()->warning('Invalid language code {langcode}', $messageArgs);
        continue;
      }

      // Add the language definition.
      $language = ConfigurableLanguage::createFromLangcode($langcode);
      $language->save();

      // Download and import translations for the newly added language if
      // interface translation is enabled.
      if ($this->moduleHandler->moduleExists('locale')) {
        module_load_include('fetch.inc', 'locale');
        $options = _locale_translation_default_update_options();
        if ($batch = locale_translation_batch_update_build([], [$langcode], $options)) {
          batch_set($batch);
          $batch =& batch_get();
          $batch['progressive'] = FALSE;

          // Process the batch.
          drush_backend_batch_process();
        }
      }

      $this->logger()->info('Added language: {langcode}', $messageArgs);
    }
  }

  /**
   * Enable one or more already defined languages.
   *
   * @param array $langcodes
   *   A comma-separated list of langcodes which will be enabled.
   *
   * @command language:enable
   *
   * @aliases langen,language-enable
   */
  public function enable(array $langcodes) {
    $langcodes = StringUtils::csvToArray($langcodes);

    if (empty($langcodes)) {
      $this->logger()->error('Please provide one or more comma-separated language codes as arguments.');
      return;
    }

    foreach ($langcodes as $langcode) {
      $messageArgs = ['langcode' => $langcode];

      // In the foreach loop because the list changes on successful iterations.
      $languages = $this->languageManager->getLanguages();

      // Skip nonexistent languages.
      if (!isset($languages[$langcode])) {
        $this->logger()->warning('Specified language does not exist {langcode}', $messageArgs);
        continue;
      }

      // Skip already-enabled languages.
      if ($languages[$langcode]->enabled) {
        $this->logger()->warning('Language already enabled: !language', $messageArgs);
        continue;
      }

      // FIXME find the D8 equivalent: this is D7 logic.
      db_update('languages')
        ->condition('language', $langcode)
        ->fields([
          'enabled' => 1,
        ])
        ->execute();

      // FIXME probably needs a more generic invalidation.
      // Changing the language settings impacts the interface.
      $this->cachePage->deleteAll();
      $this->logger()->info('Enabled language : {langcode}', $messageArgs);
    }
  }

  /**
   * Disable one or more already defined languages.
   *
   * @param array $langcodes
   *   The comma-separated langcodes of the languages which will be disabled.
   *
   * @command language:disable
   *
   * @aliases langdis,language-disable
   */
  public function disable(array $langcodes) {
    $langcodes = StringUtils::csvToArray($langcodes);

    if (empty($langcodes)) {
      $this->logger()->error('Please provide one or more comma-separated language codes as arguments.');
      return;
    }

    foreach ($langcodes as $langcode) {
      $messageArgs = ['langcode' => $langcode];
      // In the foreach loop because the list changes on successful iterations.
      $languages = $this->languageManager->getLanguages();

      // Skip nonexistent languages.
      if (!isset($languages[$langcode])) {
        $this->logger()->warning('Specified language does not exist {langcode}', $messageArgs);
        continue;
      }

      // Skip locked languages.
      if ($languages[$langcode]->isLocked()) {
        $this->logger()->warning('No disabling lock specified language {langcode}', $messageArgs);
        continue;
      }

      // Skip already-disabled languages.
      if (!$languages[$langcode]->enabled) {
        $this->logger()->warning('Language already disabled: {langcode}', $messageArgs);
        continue;
      }

      // FIXME find the D8 equivalent: this is D7 logic.
      db_update('languages')
        ->condition('language', $langcode)
        ->fields([
          'enabled' => 0,
        ])
        ->execute();

      // FIXME probably needs a more generic invalidation.
      // Changing the language settings impacts the interface.
      $this->cachePage->deleteAll();
      $this->logger()->info('Disabled language : {langcode}', $messageArgs);
    }
  }

  /**
   * Assign an enabled language as default.
   *
   * @param string $langcode
   *   The langcode of the language which will be set as the default language.
   *
   * @command language:default
   *
   * @aliases langdef,language-default
   */
  public function languageDefault($langcode) {
    $languages = $this->languageManager->getLanguages();
    if (!isset($languages[$langcode])) {
      $this->logger()->warning('Specified language does not exist {langcode}', [
        'langcode' => $langcode,
      ]);
    }

    $default_language = ConfigurableLanguage::load($langcode);
    $default_language->set('default', TRUE);
    $default_language->save();
    $this->logger()->info('{langcode} assigned as default', [
      'langcode' => $langcode,
    ]);
  }

  /**
   * Import a single .po file.
   *
   * @param string $langcode
   *   The langcode of the language in which the string will be imported.
   * @param string $poFile
   *   Path to a .po file containing the translation. Can be multiple.
   * @param array $options
   *   Import options.
   *
   * @command language:import:translations
   *
   * @option replace Replace existing translations.
   * @usage Import multiple files
   *   drush langimp eo file1.po file2.po ...
   * @usage Import with replacement
   *   drush langimp eo file.po --replace
   * @aliases langimp,language-import,language-import-translations
   *
   * @see \Drupal\locale\Form\ImportForm::submitForm
   *
   * @todo Implement \Drupal\locale\Form\ImportForm::buildForm
   * @todo This can be simplified once https://www.drupal.org/node/2631584
   *   lands
   *   in Drupal core.
   */
  public function importTranslations(
    $langcode,
    $poFile,
    array $options = ['replace' => NULL]
  ) {
    $args = func_get_args();
    if (count($args) < 2) {
      drush_set_error(dt('Usage: drush language-import <langcode> <path_to/file.po> [--replace]'));
      return;
    }
    $this->moduleHandler->loadInclude('locale', 'translation.inc');
    $this->moduleHandler->loadInclude('locale', 'bulk.inc');

    // Get arguments and options.
    $langcode = array_shift($args);
    $file_paths = $args;

    // Add language, if not yet supported.
    $languages = $this->languageManager->getLanguages();

    if (!isset($languages[$langcode])) {
      $predefined = $this->countryManager->getList();
      $this->entityTypeManager
        ->getStorage('configurable_language')
        ->create(['langcode' => $langcode]);
      drush_log(dt('The language %language has been created.',
        ['%language' => dt($predefined[$langcode])]));
    }

    $options = array_merge(_locale_translation_default_update_options(), [
      'langcode' => $langcode,
      'customized' => LOCALE_CUSTOMIZED,
      'overwrite_options' => drush_get_option('replace') ? [
        'customized' => 1,
        'not_customized' => 1,
      ] : [
        'customized' => 0,
        'not_customized' => 1,
      ],
    ]);

    // Import language files.
    $files = [];
    $langcodes = [];
    foreach ($file_paths as $file_path) {
      // Probably not an absolute path.
      // Test from the original $cwd.
      if (!file_exists($file_path)) {
        $file_path = drush_get_context('DRUSH_DRUPAL_ROOT') . '/' . $file_path;
      }

      // Ensure we have the file intended for upload.
      if (file_exists($file_path)) {
        // Create file.
        $file = locale_translate_file_create($file_path);
        // Extract project, version and language code from the file name
        // Supported:
        // - {project}-{version}.{langcode}.po, {prefix}.{langcode}.po
        // - {langcode}.po
        // or from user input.
        $file = locale_translate_file_attach_properties($file, $options);
        if ($file->langcode == LanguageInterface::LANGCODE_NOT_SPECIFIED) {
          $file->langcode = $langcode;
          if (empty($file->version) && !empty($file->project) && !empty($file->langcode)) {
            $sources = locale_translation_get_status();
            $source = $sources[$file->project][$file->langcode];
            if (isset($source->version)) {
              $file->version = $source->version;
            }
          }
        }
        $langcodes[] = $file->langcode;
        $files[] = $file;
      }
      else {
        $variables = ['@filepath' => $file_path];
        drush_log(dt('File to import at @filepath not found.', $variables),
          'error');
      }
    }

    $batch = locale_translate_batch_build($files, $options);
    batch_set($batch);

    // Create or update all configuration translations for this language.
    $langcodes = array_unique($langcodes);
    if ($batch = locale_config_batch_update_components($options, $langcodes)) {
      batch_set($batch);
    }

    drush_backend_batch_process();
    drush_log('Import complete.', 'success');
  }

  /**
   * Export string of a language as a .po file.
   *
   * @param string $langcode
   *   The langcode of the language to exported.
   * @param string $poFile
   *   Path to a .po file. Use "-" or /dev/stdin to use standard output.
   * @param array $options
   *   Export options.
   *
   * @command language:export:translations
   *
   * @option status The statuses to export, defaults to 'customized'.
   * This can be a comma-separated list of 'customized', 'not-customized',
   *   'not-translated', or 'all'.
   * @usage Export the french translation
   *   drush langexp fr fr.po
   * @aliases langexp,language-export,language-export-translations
   *
   * @todo Implement \Drupal\locale\Form\ExportForm::buildForm
   * @todo This can be simplified once https://www.drupal.org/node/2631584
   *   lands
   *   in Drupal core.
   */
  public function exportTranslations(
    $langcode,
    $poFile,
    array $options = ['status' => NULL]
  ) {
    $args = func_get_args();
    if (count($args) < 2) {
      drush_set_error(dt('Usage: drush language-export <langcode> <path_to/file.po>'));
      return;
    }

    // Get arguments and options.
    $langcode = array_shift($args);
    $file_path_arg = array_shift($args);

    // Ensure the langcode match an existing language.
    $language = $this->languageManager->getLanguage($langcode);
    if ($language == NULL) {
      drush_set_error(dt('drush language-export: no such language'));
      return;
    }

    // Validate export statuses.
    $export_statuses_allowed = [
      // Internal-value => input-value.
      'customized' => 'customized',
      'not_customized' => 'not-customized',
      'not_translated' => 'not-translated',
    ];
    $export_statuses_input = drush_get_option_list('status', 'customized');
    $export_statuses_input = array_values($export_statuses_input);
    if ($export_statuses_input == ['all']) {
      $export_statuses_input = $export_statuses_allowed;
    }
    $export_statuses_unknown = array_diff($export_statuses_input,
      $export_statuses_allowed);
    if ($export_statuses_unknown) {
      $t_args = ['@options' => implode(', ', $export_statuses_unknown)];
      return drush_set_error(dt('drush language-export: Unknown status options: @options',
        $t_args));
    }
    $export_statuses_filtered = array_intersect($export_statuses_allowed,
      $export_statuses_input);
    $export_statuses = array_fill_keys(array_keys($export_statuses_filtered),
      TRUE);

    // Relative path should be relative to cwd(), rather than Drupal root-dir.
    if (drush_is_absolute_path($file_path_arg)) {
      $file_path = $file_path_arg;
    }
    else {
      $file_path = drush_get_context('DRUSH_DRUPAL_ROOT') . DIRECTORY_SEPARATOR . $file_path_arg;
    }

    // Check if file_path exists and is writable.
    $dir = dirname($file_path);
    if (!file_prepare_directory($dir)) {
      file_prepare_directory($dir,
        FILE_MODIFY_PERMISSIONS | FILE_CREATE_DIRECTORY);
    }

    $reader = new PoDatabaseReader();
    $language_name = '';
    if ($language != NULL) {
      $reader->setLangcode($language->getId());
      $reader->setOptions($export_statuses);
      $languages = $this->languageManager->getLanguages();
      $language_name = isset($languages[$language->getId()]) ? $languages[$language->getId()]->getName() : '';
    }
    $item = $reader->readItem();
    if (!empty($item)) {
      $header = $reader->getHeader();
      $header->setProjectName($this->configFactory->get('system.site')->get('name'));
      $header->setLanguageName($language_name);

      $writer = new PoStreamWriter();
      $writer->setUri($file_path);
      $writer->setHeader($header);

      $writer->open();
      $writer->writeItem($item);
      $writer->writeItems($reader);
      $writer->close();

      drush_log('Export complete.', 'success');
    }
    else {
      drush_set_error(dt('Nothing to export.'));
    }
  }

  /**
   * Export all translations to files.
   *
   * @param array $options
   *   Export options.
   *
   * @command language:export:all:translations
   *
   * @option langcodes The language codes to export. Defaults to all enabled
   *   languages.
   * @option file-pattern The target file pattern. Defaults to
   *   'translations/custom/%language.po'. Note that this is the only place
   *   where this module's auto-importing works.
   * @option all If set, exports all translations instead of only customized
   *   ones.
   *
   * @aliases langexpall,language-export-all,language-export-all-translations
   */
  public function exportAllTranslations(
    array $options = [
      'langcodes' => NULL,
      'file-pattern' => NULL,
      'all' => NULL,
    ]
  ) {
    $langcodes = drush_get_option_list('langcodes');
    if (!$langcodes) {
      $languages = $this->languageManager->getLanguages();
      $langcodes = array_keys($languages);
    }
    $file_pattern = drush_get_option('file_pattern',
      'translations/custom/%language.po');
    $options = ['status' => drush_get_option('all') ? 'all' : 'customized'];
    foreach ($langcodes as $langcode) {
      $file_path_relative = preg_replace('/%language/u', $langcode,
        $file_pattern);
      $file_name_absolute = drush_is_absolute_path($file_path_relative) ?
        $file_path_relative : drush_get_context('DRUSH_DRUPAL_ROOT') . '/' . $file_path_relative;
      drush_invoke_process('@self', 'language-export', [
        $langcode,
        $file_name_absolute,
      ], $options);
      $t_args = ['langcode' => $langcode, '!file' => $file_path_relative];
      drush_log(dt('Exported translations for language {langcode} to file !file.',
        $t_args), 'ok');
    }
  }

}
