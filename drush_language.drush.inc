<?php
use Drupal\Core\Language\LanguageInterface;
use Drupal\language\Entity\ConfigurableLanguage;
use Drupal\locale\Gettext;
use Drupal\locale\PoDatabaseReader;
use Drupal\Component\Gettext\PoStreamWriter;

/**
 * Implementation of hook_drush_command().
 *
 * @See drush_parse_command() for a list of recognized keys.
 *
 * @return
 *   An associative array describing your command(s).
 */
function drush_language_drush_command() {
  $items = array();
  $items['language-add'] = array(
    'description' => "Add and import a new language definition",
    'arguments'   => array(
      'langcode'    => dt('The langcode of the language for which a definition will be added.'),
    ),
    'aliases' => array('langadd'),
  );
  $items['language-enable'] = array(
    'description' => "Enable an already defined language",
    'arguments'   => array(
      'langcode'    => dt('The langcode of the language which will be enabled.'),
    ),
    'aliases' => array('langen'),
  );
  $items['language-disable'] = array(
    'description' => "Disable an already defined language",
    'arguments'   => array(
      'langcode'    => dt('The langcode of the language which will be disabled.'),
    ),
    'aliases' => array('langdis'),
  );
  $items['language-default'] = array(
    'description' => "Assign an enabled language as default",
    'arguments'   => array(
      'langcode'    => dt('The langcode of the language which will be set as the default language.'),
    ),
    'aliases' => array('langdef'),
  );
  $items['language-import'] = array(
    'description' => "Import a single .po file",
    'arguments'   => array(
      'langcode'    => dt('The langcode of the language in which the string will be imported.'),
      '.po file'    => dt('Path to a .po file containing the translation. Can be multiple.'),
    ),
    'examples' => array(
      'Import multiple files' => 'drush langimp eo file1.po file2.po ...',
      'Import with replacement' => 'drush langimp eo file.po --replace',
      'Import with custom group' => 'drush langimp eo file.po --group=mygroup',
    ),
    'options' => array(
      'replace' => array(
        'description' => dt('Replace existing translations.'),
        'value' => 'optional'
      ),
      'group' => array(
        'description' => dt('The group to which the strings should be imported, defaults to \'default\'.'),
        'value' => 'optional'
      ),
    ),
    'aliases' => array('langimp'),
  );
  $items['language-export'] = array(
    'description' => "Export string of a language as a .po file",
    'arguments'   => array(
      'langcode'    => dt('The langcode of the language to exported.'),
      '.po file'    => dt('Path to a .po file. Use "-" or /dev/stdin to use standard output.'),
    ),
    'examples' => array(
      'Export the french translation of the translatable menu items' => 'drush langexp --group=menu fr menu.fr.po',
    ),
    'options' => array(
      'group' => array(
        'description' => dt('The text-group to export, defaults to \'default\'.' . "\n" .
			    'According to enabled modules this could be "blocks", "menu", "taxonomy", "countries", ...'),
        'value' => 'optional'
      ),
    ),
    'aliases' => array('langexp'),
  );
  return $items;
}

/**
 * Add a language
 */
function drush_drush_language_language_add() {
  if (!\Drupal::moduleHandler()->moduleExists('language')) {
    drush_set_error(dt('Could not add language. Is the \'language\' module enabled?'));
    return;
  }
  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    $language_list = \Drupal::languageManager()->getLanguages();
    if (array_key_exists($langcode, $language_list)) {
      drush_log(dt('The language with code !code already exists.', array('!code' => $langcode)), 'warning');
    }
    else {
      // Predefined language selection.
      $predefined = \Drupal::languageManager()->getStandardLanguageListWithoutConfigured();
      if (!isset($predefined[$langcode])) {
        drush_log(dt('Invalid language code !language', array('!language' => $langcode)), 'warning');
      }
      else {
        // Add the language definition.
        $language = ConfigurableLanguage::createFromLangcode($langcode);
        $language->save();

        // Download and import translations for the newly added language if
        // interface translation is enabled.
        if (\Drupal::moduleHandler()->moduleExists('locale')) {
          module_load_include('fetch.inc', 'locale');
          $options = _locale_translation_default_update_options();
          if ($batch = locale_translation_batch_update_build(array(), array($langcode), $options)) {
            batch_set($batch);
            $batch =& batch_get();
            $batch['progressive'] = FALSE;

            // Process the batch.
            drush_backend_batch_process();
          }
        }
        drush_log(dt('Added language: !language', array('!language' => $langcode)), 'ok');
      }
    }
  }
}

/**
 * Enable a language
 */
function drush_language_enable() {
  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    $languages = language_list();
    if (array_key_exists($langcode, $languages)) {
      if (!$languages[$langcode]->enabled) {
        // disable the default english
        db_update('languages')
          ->condition('language', $langcode)
          ->fields(array(
            'enabled' => 1,
          ))
          ->execute();

        // Changing the language settings impacts the interface.
        cache_clear_all('*', 'cache_page', TRUE);
        drush_log(dt("Enabled language : !language ", array('!language' => $langcode)), 'ok');
      }
      else {
        drush_log(dt("Language already enabled: !language ", array('!language' => $langcode)), 'warning');
      }
    }
    else {
      drush_log(dt("Specified language does not exist !language", array('!language' => $langcode)), 'warning');
    }
  }
}

/**
 * Disables a language
 */
function drush_drush_language_disable() {
  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    $language_manager = \Drupal::languageManager();
    $languages = $language_manager->getLanguages();
    if (array_key_exists($langcode, $languages)) {
      if ($languages[$langcode]->is) {
        // disable the default english
        db_update('languages')
          ->condition('language', $langcode)
          ->fields(array(
            'enabled' => 0,
          ))
          ->execute();

        // Changing the language settings impacts the interface.
        cache_clear_all('*', 'cache_page', TRUE);
        drush_log(dt("Disabled language : !language ", array('!language' => $langcode)), 'ok');
      }
      else {
        drush_print(dt("Language already disabled: !language ", array('!language' => $langcode)), 'warning');
      }
    }
  }
}

/**
 * Assigns the default language
 */
function drush_drush_language_default() {
  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    // get all the languages
    $language_manager = \Drupal::languageManager();
    $languages = $language_manager->getLanguages();
    if (array_key_exists($langcode, $languages)) {
      $default_language = ConfigurableLanguage::load($langcode);
      $default_language->set('default', TRUE);
      $default_language->save();
      drush_log(dt("!language assigned as default", array('!language' => $langcode)), 'ok');
    }
    else {
      drush_log(dt("Specified language does not exist !language", array('!language' => $langcode)), 'warning');
    }
  }
}

/**
 * Imports .po file(s).
 *
 * @see \Drupal\locale\Form\ImportForm::submitForm
 *
 * @todo Implement \Drupal\locale\Form\ImportForm::buildForm
 * @todo This can be simplified once https://www.drupal.org/node/2631584 lands
 *   in Drupal core.
 */
function drush_drush_language_language_import() {
  $args = func_get_args();
  if (count($args) < 2) {
    drush_set_error(dt('Usage: drush language-import <langcode> <path_to/file.po> [--replace] [--group=group]'));
    return;
  }
  $module_handler = \Drupal::moduleHandler();
  $module_handler->loadInclude('locale', 'translation.inc');
  $module_handler->loadInclude('locale', 'bulk.inc');

  // Get arguments and options.
  $langcode = array_shift($args);
  $file_paths = $args;
  // Overwrite existing customized and non-customized translations.
  $replace_arg = drush_get_option('replace');
  $overwrite_options = isset($replace_arg) ? ['customized' => TRUE] : ['not_customized' => TRUE];
  // Translation project (interface_translation_properties).
  $group = drush_get_option('group', 'default');
  if ($group === TRUE) {
    drush_set_error(dt('drush language-import --group option expects a string value'));
    return;
  }

  // Add language, if not yet supported.
  $languages = \Drupal::languageManager()->getLanguages();

  if (!isset($languages[$langcode])) {
    /** @var \Drupal\Core\Locale\CountryManagerInterface $country_manager */
    $country_manager = \Drupal::service('country_manager');
    $predefined = $country_manager->getList();
    \Drupal::entityTypeManager()->getStorage('configurable_language')->create(['langcode' => $langcode]);
    drush_log(dt('The language %language has been created.', array('%language' => dt($predefined[$langcode]))));
  }

  $options = array_merge(_locale_translation_default_update_options(), [
    'langcode' => $langcode,
    'customized' => LOCALE_CUSTOMIZED,
    'overwrite_options' => isset($replace_arg) ? [
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
      if ($file->langcode = LanguageInterface::LANGCODE_NOT_SPECIFIED) {
        $file->langcode = $langcode;
        if ($group != 'default') {
          $file->project = $group;
        }
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
      drush_log(dt('File to import at @filepath not found.', $variables), 'error');
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
 * Exports .po file(s).
 *
 * @see \Drupal\locale\Form\ExportForm::submitForm
 *
 * @todo Implement \Drupal\locale\Form\ExportForm::buildForm
 * @todo This can be simplified once https://www.drupal.org/node/2631584 lands
 *   in Drupal core.
 */
function drush_drush_language_language_export() {
  $args = func_get_args();
  if (count($args) < 2) {
    drush_set_error(dt('Usage: drush language-export <langcode> <path_to/file.po> [--group=group]'));
    return;
  }

  // Get arguments and options.
  $langcode = array_shift($args);
  $file_path_arg = array_shift($args);
  $group = drush_get_option('group', 'default');
  if ($group === TRUE) {
    drush_set_error(dt('drush language-export --group option expects a string value'));
    return;
  }

  // Ensure the langcode match an existing language.
  $language = \Drupal::languageManager()->getLanguage($langcode);
  if ($language == NULL) {
    drush_set_error(dt('drush language-export: no such language'));
    return;
  }

  // Relative path should be relative to cwd(), rather than Drupal root-dir.
  if (drush_is_absolute_path($file_path_arg)) {
    $file_path = $file_path_arg;
  }
  else {
    $file_path = drush_get_context('DRUSH_DRUPAL_ROOT') . DIRECTORY_SEPARATOR . $file_path_arg;
  }

  // Check if file_path exists and is writable.
  $dir = dirname($file_path);
  if(!file_prepare_directory($dir)) {
    file_prepare_directory($dir, FILE_MODIFY_PERMISSIONS | FILE_CREATE_DIRECTORY);
  }

  $reader = new PoDatabaseReader();
  $language_name = '';
  if ($language != NULL) {
    $reader->setLangcode($language->getId());
    $reader->setOptions([
      // Include non-customized translations.
      'not_customized' => TRUE,
      // Include customized translations.
      'customized' => TRUE,
      // Include untranslated text.
      'not_translated' => TRUE,

    ]);
    $languages = \Drupal::languageManager()->getLanguages();
    $language_name = isset($languages[$language->getId()]) ? $languages[$language->getId()]->getName() : '';
  }
  $item = $reader->readItem();
  if (!empty($item)) {
    $header = $reader->getHeader();
    $header->setProjectName(\Drupal::config('system.site')->get('name'));
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