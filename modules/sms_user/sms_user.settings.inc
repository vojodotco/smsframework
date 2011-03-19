<?php

/** 
 * @file
 *   SMS Framework user module. User settings, add, remove, verify numbers.
 */

variable_set('sms_user_multiple_numbers', TRUE);

/**
 * Form: User numbers overview.
 *
 * @param $account
 *   User object.
 */
function sms_user_settings_page($account) {
  $content = array();

  /**
   * First add a number (and check if a number has been added).
   */
  $content['add'] = drupal_get_form('sms_user_settings_add_form', $account);
  

  /**
   * Check for any validations done or todo.
   */
  $content['validate'] = drupal_get_form('sms_user_settings_confirm_form', $account, $number);

  /**
   * List active numbers,
   */
  $content['active'] = drupal_get_form('sms_user_settings_active_form', $account);

  /**
   * Sleep.
   */
  if (variable_get('sms_user_sleep', 1)) {
    $content['sleep'] = drupal_get_form('sms_user_settings_sleep_form', $account, TRUE);
  }
  
  return theme('sms_user_settings_page', $content);
}

/**
 * Form: Add a new mobile number to an account.
 *
 * @param $account 
 *   User object to add number to.
 * @param $fieldset
 *   Bool TRUE if form should be in fieldset.
 */
function sms_user_settings_add_form(&$form_state, $account) {
  dsm('sms_user_settings_add_form');
  // refresh the numbers, one can just be added, no form if number and not multiple
  $numbers = sms_user_numbers($account, 'numbers', TRUE);
  if (count($numbers) && ! variable_get('sms_user_multiple_numbers', FALSE)) {
    return;
  }

  $form_state['storage']['uid'] = $account->uid;

  $form = array();
  $form['add'] = sms_send_form();
  $form['add']['add_submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add number'),
  );

  // place in a fieldset if there is more than one number 
  if (count($numbers)) {
    $form['add']['#type'] = 'fieldset';
    $form['add']['#title'] = t('Add another number');
    $form['add']['#collapsible'] = TRUE;
    $form['add']['#collapsed'] = TRUE;
  }
  return $form;
}

function sms_user_settings_add_form_validate($form, &$form_state) {
  dsm('sms_user_settings_add_form_validate');
  if ($error = sms_user_validate_number($form_state['values']['number'])) {
    form_set_error('number', $error);
  }

  if (empty($form_state['values']['gateway'])) {
   $form_state['values']['gateway'] = array();
  }
}

function sms_user_settings_add_form_submit($form, &$form_state) {
  dsm('sms_user_settings_add_form_submit');
  $account = user_load(array('uid' => $form_state['storage']['uid']));
  sms_user_send_confirmation($account, $form_state['values']['number'], $form_state['values']['gateway']);
}

/**
 * Form: Confirm a mobile number.
 * 
 * @param $account
 *   User object.
 * @param $number
 *   Number array (@todo or delta?)
 */
function sms_user_settings_confirm_form(&$form_state, $account, $number) {
  dsm('sms_user_settings_confirm_form');
  // get and reset numbers list, return if none pending confirmation.
  $numbers = sms_user_numbers($account, 'status', TRUE);
  if (! count($numbers[SMS_USER_PENDING])) {
    return;
  }

  // force a reload of the numbers, if just changed gets new account
  $form_state['storage']['uid'] = $account->uid;

  foreach ($numbers[SMS_USER_PENDING] as $number) {
    $form[$number['number']]['confirm_number[' . $number['number'] .']'] = array(
      '#type' => 'item',
      '#title' => t('Mobile phone number'),
      '#value' => $number['number'],
    );
    $form[$number['number']]['confirm_code[' . $number['number'] .']'] = array(
      '#type' => 'textfield',
      '#title' => t('Confirmation code'),
      '#description' => t('The code sent by SMS to your mobile phone.'),
      '#size' => 4,
      '#maxlength' => 4,
      '#parent' => $number['number'],
    );
    $form[$number['number']]['confirm_submit[' . $number['number'] . ']'] = array(
      '#type' => 'submit',
      '#value' => t('Confirm'),
    );
    $form[$number['number']]['confirm_delete[' . $number['number'] .']'] = array(
      '#type' => 'item',
      '#value' => l('Delete', 'user/' . $form_state['storage']['uid'] . '/edit/mobile/' . $number['number'] . '/delete', array('query' => drupal_get_destination())),
      '#access' => user_access('edit own sms number'),
    );
  }

  return $form;
}

function sms_user_settings_confirm_form_validate($form, &$form_state) {
  dsm('sms_user_settings_confirm_form_validate');
  dsm($form_state);
  $account = user_load(array('uid' => $form_state['storage']['uid']));
  $filled = FALSE;
  foreach($form_state['clicked_button']['#post']['confirm_code'] as $number => $code) {
    if (! empty($code)) {
      $filled = TRUE;
      if (! sms_user_confirm($account, $number, $code, TRUE)) {
        form_set_error('confirm_code[' . $number . ']', t('The confirmation code for number %number is invalid.', array('%number' => $number)));
      }
    }
  }
  // @todo do we want this message, if so it might need attaching to something.
  if (! $filled) {
    form_set_error(NULL, t('To confirm a number you must enter the code sent by SMS to you mobile phone.'));
  }
}

function sms_user_settings_confirm_form_submit($form, &$form_state) {
  dsm('sms_user_settings_confirm_form_submit');
  $account = user_load(array('uid' => $form_state['storage']['uid']));
  foreach($form_state['clicked_button']['#post']['confirm_code'] as $number => $code) {
   if (! empty($code)) {
     sms_user_confirm($account, $number, $code);
   }
 }
}

/**
 * Form: Lists confirmed number(s). For multiples allows setting active number.
 */
function sms_user_settings_active_form(&$form_state, $account) {
  dsm('sms_user_settings_active_form');
  $form_state['storage']['uid'] = $account->uid;
  $form = array();

  // we want to list numbers in a consistent order.
  $numbers = sms_user_numbers($account, 'number', TRUE);
  ksort($numbers);

  foreach ($numbers as $number) {
    if ($number['status'] == SMS_USER_PENDING) {
      // skip numbers that aren't confirmed.
      continue;
    }
    $row = array();
    $row['number'] = array(
      '#type' => 'item',
      '#title' => t('Your mobile phone number'),
      '#value' => $number['number'],
      '#description' => t('Your mobile phone number has been confirmed.')
    );
    if (variable_get('sms_user_multiple_numbers', FALSE)) {
      $options[$number['number']] = '';
      if ($number['status'] == SMS_USER_ACTIVE) {
        $active = $number['number'];
      }
    }
    $row['confirm_delete'] = array(
      '#type' => 'item',
      '#value' => l('Delete', 'user/' . $form_state['storage']['uid'] . '/edit/mobile/' . $number['number'] . '/delete', array('query' => drupal_get_destination())),
      '#access' => user_access('edit own sms number'),
    );
    $form[$number['number']] = $row;
  }

  // if there is more than one number, add radio buttons to select the active one.
  if (variable_get('sms_user_multiple_numbers', FALSE)) {
    $form['active'] = array(
      '#type' => 'radios',
      '#options' => $options,
      '#default_value' => $active,
    );
    if (count($options)) {
      $form['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Set active'),
      );
    }
  }

  return $form;
}

function sms_user_settings_active_form_submit($form, &$form_state) {
  dsm('sms_user_settings_active_form_submit');
  sms_user_active_set($form_state['storage']['uid'], $form_state['values']['active']);
}

/**
 * Form: Delete number from account.
 */
function sms_user_settings_delete_form(&$form_state, $account, $number) {
  $form_state['storage']['uid'] = $account->uid;
  $form_state['storage']['number'] = $number;
  $form = array();

  return confirm_form($form,
    t('Are you sure you want to delete mobile number %number?', array('%number' => $number)),
    isset($_GET['destination']) ? $_GET['destination'] : 'user/'. $account->uid .'/edit/mobile',
    NULL,
    t('Delete'),
    t('Cancel')
  );
}

function sms_user_settings_delete_form_submit($form, &$form_state) {
  if ($form_state['clicked_button']['#value'] == t('Delete')) {
    sms_user_delete($form_state['storage']['uid'], $form_state['storage']['number']);
  }
  drupal_goto();
}

/**
 * Form: Account mobile sleep settings.
 *
 * @param $account
 *   User object.
 * @param $fieldset
 *   Bool TRUE if should be in a fieldset.
 */
function sms_user_settings_sleep_form(&$form_state, $account, $fieldset = FALSE) {
  dsm('sms_user_settings_sleep_form');
  $form_state['storage']['uid'] = $account->uid;

  $form['sleep']['sleep_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable messages between these hours'),
    '#description' => t('If enabled, you will not receive messages between the specified hours.'),
    '#default_value' => isset($account->sms_user['sleep_enabled']) ? $account->sms_user['sleep_enabled'] : NULL,
  );

  // Determine whether to use the 24-hour or 12-hour clock based on site settings
  if (strpos(variable_get('date_format_short', 'm/d/Y - H:i'), 'g')) {
    $format = 'g A';
  }
  else {
    $format = 'H:00';
  }
  // Build the list of options based on format
  $hour = 0;
  while ($hour < 24) {
    $options[$hour] = date($format, mktime($hour));
    $hour++;
  }

  $form['sleep']['sleep_start_time'] = array(
    '#type' => 'select',
    '#multiple' => FALSE,
    '#options' => $options,
    '#default_value' => isset($account->sms_user['sleep_start_time']) ? $account->sms_user['sleep_start_time'] : NULL,
  );

  $form['sleep']['sleep_end_time'] = array(
    '#type' => 'select',
    '#multiple' => FALSE,
    '#options' => $options,
    '#default_value' => isset($account->sms_user['sleep_end_time']) ? $account->sms_user['sleep_end_time'] : NULL,
  );

  $form['sleep']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  if ($fieldset) {
    $form['sleep']['#type'] = 'fieldset';
    $form['sleep']['#title'] = t('Set sleep Time');
    $form['sleep']['#collapsible'] = TRUE;
    $form['sleep']['#collapsed'] = TRUE;
  }

  return $form;
}

function sms_user_settings_sleep_form_submit($form, &$form_state) {
  dsm('sms_user_settings_sleep_form_submit');
  $account = user_load(array('uid' => $form_state['storage']['uid']));
  $data = $account->sms_user;
  $data['sleep_enabled'] = $form_state['values']['sleep_enabled'];
  $data['sleep_start_time'] = $form_state['values']['sleep_start_time'];
  $data['sleep_end_time'] = $form_state['values']['sleep_end_time'];
  user_save($account, array('sms_user' => $data), 'mobile');
  drupal_set_message(t('The changes have been saved.'), 'status');
}

/********
 * Theme Functions
 */

/**
 * Theme function for combined user settings page.
 *
 * @param $content
 *   array of forms: active, validate, add and sleep.
 * @ingroup themeable
 */
function theme_sms_user_settings_page($content) {
  $output = $content['active'];
  $output .= $content['validate'];
  $output .= $content['add']; 
  $output .= $content['sleep'];
  return $output;
}

/**
 * Theme function for the confirm numbers form.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @ingroup themeable
 */
function theme_sms_user_settings_confirm_form($form) {
  $output = '';

  if (variable_get('sms_user_multiple_numbers', FALSE)) {
    $rows = array();

    foreach (element_children($form) as $key) {
      if (! isset($form[$key]['confirm_number[' . $key . ']'])) {
        // just putting the numbers (and their selector) in a table
        continue;
      }

      $number = $form[$key];
      unset($form[$key]);

      unset($number['confirm_code[' . $key . ']']['#title']);
      unset($number['confirm_code[' . $key . ']']['#description']);

      $row = array();
      $row[] = $number['confirm_number[' . $key . ']']['#value'];
      $row[] = drupal_render($number['confirm_code[' . $key . ']']);
      $row[] = drupal_render($number['confirm_submit[' . $key . ']']);
      $row[] = drupal_render($number['confirm_delete[' . $key . ']']);
      $rows[] = $row;

    }

    if (count($rows)) {
      $header = array(t('Mobile phone number'), t('Confirmation code'), '', '');
      $output .= theme('table', $header, $rows);
      $output .= t('The confirmation code sent by SMS to your mobile phone.');
    }
  }

  $output .= drupal_render($form); 
  return $output;
}


/**
 * Theme function for the active numbers form.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @ingroup themeable
 */
function theme_sms_user_settings_active_form($form) {
  $output = '';

  if (variable_get('sms_user_multiple_numbers', FALSE)) {
    $rows = array();

    foreach (element_children($form) as $key) {
      if (! isset($form[$key]['number'])) {
        // just putting the numbers (and their selector) in a table
        continue;
      }
      $number = $form[$key];
      unset($form[$key]);

      $row = array();
      $row[] = $number['number']['#value'];
      $row[] = drupal_render($form['active'][$key]);
      $row[] = drupal_render($number['confirm_delete']);
      $rows[] = $row;
    }

    if (count($rows)) {
      $header = array(t('Mobile phone number'), t('Active'), '');
      $output .= theme('table', $header, $rows);
      $output .= t('You can select the <em>active</em> number to receive SMS from the site. All numbers will be able to send SMS to the site.');
    }
  }

  $output .= drupal_render($form); 
  return $output;
}

